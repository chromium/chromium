// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_PROTO_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_PROTO_UTIL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine_impl/cycle/sync_cycle.h"

namespace sync_pb {
class ClientToServerMessage;
class ClientToServerResponse;
class ClientToServerResponse_Error;
class CommitResponse_EntryResponse;
class SyncEntity;
}

namespace syncer {

class ServerConnectionManager;
struct SyncProtocolError;

// Returns the types to migrate from the data in |response|.
ModelTypeSet GetTypesToMigrate(const sync_pb::ClientToServerResponse& response);

// Builds a SyncProtocolError from the data in |error|.
SyncProtocolError ConvertErrorPBToSyncProtocolError(
    const sync_pb::ClientToServerResponse_Error& error);

class SyncerProtoUtil {
 public:
  // Adds all fields that must be sent on every request, which includes store
  // birthday, protocol version, client chips, api keys, etc. |msg| must be not
  // null. Must be called before calling PostClientToServerMessage().
  static void AddRequiredFieldsToClientToServerMessage(
      const SyncCycle* cycle,
      sync_pb::ClientToServerMessage* msg);

  // Posts the given message and fills the buffer with the returned value.
  // Returns true on success.  Also handles store birthday verification: will
  // produce a SyncError if the birthday is incorrect. Before calling this
  // method, AddRequiredFieldsToClientToServerMessage() must be called.
  static SyncerError PostClientToServerMessage(
      const sync_pb::ClientToServerMessage& msg,
      sync_pb::ClientToServerResponse* response,
      SyncCycle* cycle,
      ModelTypeSet* partial_failure_data_types);

  // Specifies where entity's position should be updated from the data in
  // GetUpdates message.
  static bool ShouldMaintainPosition(const sync_pb::SyncEntity& sync_entity);

  // Specifies where entity's parent ID should be updated from the data in
  // GetUpdates message.
  static bool ShouldMaintainHierarchy(const sync_pb::SyncEntity& sync_entity);

  // Extract the name field from a sync entity.
  static const std::string& NameFromSyncEntity(
      const sync_pb::SyncEntity& entry);

  // Extract the name field from a commit entry response.
  static const std::string& NameFromCommitEntryResponse(
      const sync_pb::CommitResponse_EntryResponse& entry);

  // Get a debug string representation of the client to server response.
  static std::string ClientToServerResponseDebugString(
      const sync_pb::ClientToServerResponse& response);

  // Get update contents as a string. Intended for logging, and intended
  // to have a smaller footprint than the protobuf's built-in pretty printer.
  static std::string SyncEntityDebugString(const sync_pb::SyncEntity& entry);

  // Set the protocol version field in the outgoing message.
  static void SetProtocolVersion(sync_pb::ClientToServerMessage* msg);

 private:
  SyncerProtoUtil() {}

  // Helper functions for PostClientToServerMessage.

  // Analyzes error fields and store birthday in response message, compares
  // store birthday with the one in the sync cycle and returns corresponding
  // SyncProtocolError. If needed updates store birthday in the cycle context.
  // This function makes it easier to test error handling.
  static SyncProtocolError GetProtocolErrorFromResponse(
      const sync_pb::ClientToServerResponse& response,
      SyncCycleContext* context);

  // Returns true if sync is disabled by admin for a dasher account.
  static bool IsSyncDisabledByAdmin(
      const sync_pb::ClientToServerResponse& response);

  // Post the message using the scm, and do some processing on the returned
  // headers. Decode the server response.
  static bool PostAndProcessHeaders(ServerConnectionManager* scm,
                                    SyncCycle* cycle,
                                    const sync_pb::ClientToServerMessage& msg,
                                    sync_pb::ClientToServerResponse* response);

  static base::TimeDelta GetThrottleDelay(
      const sync_pb::ClientToServerResponse& response);

  friend class LoopbackServerTest;
  friend class SyncerProtoUtilTest;
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, AddRequestBirthday);
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, PostAndProcessHeaders);
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, HandleThrottlingNoDatatypes);
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, HandleThrottlingWithDatatypes);

  DISALLOW_COPY_AND_ASSIGN(SyncerProtoUtil);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_PROTO_UTIL_H_
