/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "StatusServer.h"
#include "JSONConversion.h"
#include "libNetwork/Blacklist.h"

using namespace jsonrpc;
using namespace std;

string StatusServer::GetLatestEpochStatesUpdated() {
  LOG_MARKER();
  uint64_t epochNum;
  if (!BlockStorage::GetBlockStorage().GetLatestEpochStatesUpdated(epochNum)) {
    return "";
  }
  return to_string(epochNum);
}

Json::Value StatusServer::GetDSCommittee() {
  if (m_mediator.m_DSCommittee == NULL) {
    throw JsonRpcException(RPC_INTERNAL_ERROR, "DS Committee empty");
  }

  lock_guard<mutex>(m_mediator.m_mutexDSCommittee);

  const DequeOfNode& dq = *m_mediator.m_DSCommittee;

  return JSONConversion::convertDequeOfNode(dq);
}

bool StatusServer::AddToBlacklistExclusion(const string& ipAddr) {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }

  try {
    boost::multiprecision::uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    if (!Blacklist::GetInstance().Exclude(numIP)) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Could not add IP Address in exclusion list, already present");
    }

    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::RemoveFromBlacklistExclusion(const string& ipAddr) {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }

  try {
    boost::multiprecision::uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    if (!Blacklist::GetInstance().RemoveExclude(numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Could not remove IP Address from exclusion list");
    }

    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

string StatusServer::GetNodeState() {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }
  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE) {
    return m_mediator.m_node->GetStateString();
  } else {
    return m_mediator.m_ds->GetStateString();
  }
}

Json::Value StatusServer::IsTxnInMemPool(const string& tranID) {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a lookup");
  }
  try {
    if (tranID.size() != TRAN_HASH_SIZE * 2) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Txn Hash size not appropriate");
    }

    TxnHash tranHash(tranID);
    Json::Value _json;

    switch (m_mediator.m_node->IsTxnInMemPool(tranHash)) {
      case PoolTxnStatus::NOT_PRESENT:
        _json["present"] = false;
        _json["code"] = PoolTxnStatus::NOT_PRESENT;
        return _json;
      case PoolTxnStatus::PRESENT_NONCE_HIGH:
        _json["present"] = true;
        _json["code"] = PoolTxnStatus::PRESENT_NONCE_HIGH;
        _json["info"] = "Nonce too high";
        return _json;
      case PoolTxnStatus::PRESENT_GAS_EXCEEDED:
        _json["present"] = true;
        _json["code"] = PoolTxnStatus::PRESENT_GAS_EXCEEDED;
        _json["info"] = "Could not fit in as microblock gas limit reached";
        return _json;
      case PoolTxnStatus::ERROR:
        throw JsonRpcException(RPC_INTERNAL_ERROR, "Processing transactions");
      default:
        throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
    }
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "[Error]" << e.what() << " Input " << tranID);
    throw JsonRpcException(RPC_MISC_ERROR,
                           string("Unable To Process: ") + e.what());
  }
}