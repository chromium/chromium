// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_SINGLE_TYPE_MOCK_SERVER_H_
#define COMPONENTS_SYNC_TEST_SINGLE_TYPE_MOCK_SERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/commit_and_get_updates_types.h"

namespace sync_pb {
class DataTypeProgressMarker;
class EntitySpecifics;
class SyncEntity;
class ClientToServerMessage;
class ClientToServerResponse;
class DataTypeProgressMarker;
class DataTypeContext;
}  // namespace sync_pb

namespace syncer {

// A mock server used to test of happy-path update and commit logic.
//
// This object supports only one DataType, which must be specified at
// initialization time.  It does not support GetUpdates messages.  It does not
// support simulated errors.
//
// This class is useful for testing UpdateHandlers and CommitContributors.
class SingleTypeMockServer {
 public:
  explicit SingleTypeMockServer(DataType type);

  SingleTypeMockServer(const SingleTypeMockServer&) = delete;
  SingleTypeMockServer& operator=(const SingleTypeMockServer&) = delete;

  ~SingleTypeMockServer();

  // Generates a SyncEntity representing a server-delivered update containing
  // the root node for this SingleTypeMockServer's type.
  sync_pb::SyncEntity TypeRootUpdate();

  // Generates a SyncEntity representing a server-delivered update.
  //
  // The |version_offset| parameter allows the caller to simulate reflected
  // updates, redeliveries, and genuine updates.
  sync_pb::SyncEntity UpdateFromServer(
      int64_t version_offset,
      const ClientTagHash& tag_hash,
      const sync_pb::EntitySpecifics& specifics);

  // Generates a SyncEntity representing a server-delivered update for a shared
  // type.
  sync_pb::SyncEntity UpdateFromServer(
      int64_t version_offset,
      const ClientTagHash& tag_hash,
      const sync_pb::EntitySpecifics& specifics,
      const std::string& collaboration_id);

  // Generates a SyncEntity representing a server-delivered update to delete
  // an item.
  sync_pb::SyncEntity TombstoneFromServer(int64_t version_offset,
                                          const ClientTagHash& tag_hash);

  // Generates a response to the specified commit message.
  //
  // This does not perform any exhausive testing of the sync protocol.  Many of
  // the request's fields may safely be left blank, and much of the returned
  // response will be empty, too.
  //
  // This is useful mainly for testing objects that implement the
  // CommitContributor interface.
  sync_pb::ClientToServerResponse DoSuccessfulCommit(
      const sync_pb::ClientToServerMessage& message);

  // Getters to return the commit messages sent to the server through
  // DoSuccessfulCommit().
  size_t GetNumCommitMessages() const;
  const sync_pb::ClientToServerMessage& GetNthCommitMessage(size_t n) const;

  // Getters to return the most recently committed entities for a given
  // unique_client_tag hash.
  bool HasCommitEntity(const ClientTagHash& tag_hash) const;
  sync_pb::SyncEntity GetLastCommittedEntity(
      const ClientTagHash& tag_hash) const;

  // Getters that create realistic-looking progress markers and data type
  // context.
  sync_pb::DataTypeProgressMarker GetProgress() const;
  sync_pb::DataTypeContext GetContext() const;

  // Sets the token that will be returned as part of GetProgress().
  void SetProgressMarkerToken(const std::string& token);

  // Sets whether to return GC directive as part of GetProgress().
  void SetReturnGcDirectiveVersionWatermark(bool return_gc_directive);

  // Update active collaborations.
  void AddCollaboration(const std::string& collaboration_id);
  void RemoveCollaboration(const std::string& collaboration_id);

 private:
  static std::string GenerateId(const ClientTagHash& tag_hash);

  // Get and set our emulated server state.
  int64_t GetServerVersion(const ClientTagHash& tag_hash) const;
  void SetServerVersion(const ClientTagHash& tag_hash, int64_t version);

  const DataType type_;
  const std::string type_root_id_;

  // Server version state maps.
  std::map<ClientTagHash, int64_t> server_versions_;

  // Log of messages sent to the server.
  std::vector<sync_pb::ClientToServerMessage> commit_messages_;

  // Map of most recent commits by tag_hash.
  std::map<ClientTagHash, sync_pb::SyncEntity> committed_items_;

  // The token that is used to generate the current progress marker.
  std::string progress_marker_token_;

  // Whether to return version watermark GC directive in GetProgress().
  bool return_gc_directive_version_watermark_ = false;

  // List of active collaborations for the current type. Used for shared types
  // only.
  std::set<std::string> active_collaborations_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_SINGLE_TYPE_MOCK_SERVER_H_
