// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SYNC_ENGINE_SYNC_PROTOCOL_ERROR_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_PROTOCOL_ERROR_H_

#include <memory>
#include <string>

#include "components/sync/base/data_type.h"

namespace syncer {

enum SyncProtocolErrorType {
  // Success case.
  SYNC_SUCCESS,

  // Birthday does not match that of the server.
  NOT_MY_BIRTHDAY,

  // Server is busy. Try later.
  THROTTLED,

  // Server cannot service the request now.
  TRANSIENT_ERROR,

  // Indicates the datatypes have been migrated and the client should resync
  // them to get the latest progress markers.
  MIGRATION_DONE,

  // An administrator disabled sync for this domain.
  DISABLED_BY_ADMIN,

  // Some of servers are busy. Try later with busy servers.
  PARTIAL_FAILURE,

  // Returned when server detects that this client's data is obsolete. Client
  // should reset local data and restart syncing.
  CLIENT_DATA_OBSOLETE,

  // Returned when the server detects that the encryption state (Nigori,
  // keystore keys) has been reset/overridden, which means the local
  // Nigori-related state is obsolete and should be cleared.
  ENCRYPTION_OBSOLETE,

  // The default value.
  UNKNOWN_ERROR,

  // Below are commit specific values (corresponds to
  // CommitResponse::ResponseType).
  // At least one of the entities had conflict.
  CONFLICT,

  // Server reports that the request was malformed.
  INVALID_MESSAGE
};

enum ClientAction {
  // Upgrade the client to latest version.
  UPGRADE_CLIENT,

  // Wipe this client of any sync data.
  DISABLE_SYNC_ON_CLIENT,

  // Account is disabled by admin. Stop sync, clear prefs and show message on
  // settings page that account is disabled.
  STOP_SYNC_FOR_DISABLED_ACCOUNT,

  // Generated in response to CLIENT_DATA_OBSOLETE error. SyncServiceImpl
  // should stop sync engine, delete the data and restart sync engine.
  RESET_LOCAL_SYNC_DATA,

  // The default. No action.
  UNKNOWN_ACTION
};

struct SyncProtocolError {
  SyncProtocolErrorType error_type = UNKNOWN_ERROR;
  std::string error_description;
  ClientAction action = UNKNOWN_ACTION;
  DataTypeSet error_data_types;
};

const char* GetSyncErrorTypeString(SyncProtocolErrorType type);
const char* GetClientActionString(ClientAction action);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_PROTOCOL_ERROR_H_
