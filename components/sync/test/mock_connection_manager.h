// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_CONNECTION_MANAGER_H_
#define COMPONENTS_SYNC_TEST_MOCK_CONNECTION_MANAGER_H_

#include <stdint.h>

#include <bitset>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/net/server_connection_manager.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
class ClientCommand;
class CommitMessage;
class CommitResponse;
class ClientToServerMessage;
class ClientToServerResponse;
class DataTypeProgressMarker;
class GetUpdatesResponse;
class ClientCommand;
}  // namespace sync_pb

namespace syncer {

// Mock ServerConnectionManager class for use in client unit tests.
class MockConnectionManager : public ServerConnectionManager {
 public:
  class MidCommitObserver {
   public:
    virtual void Observe() = 0;

   protected:
    virtual ~MidCommitObserver() = default;
  };

  MockConnectionManager();

  MockConnectionManager(const MockConnectionManager&) = delete;
  MockConnectionManager& operator=(const MockConnectionManager&) = delete;

  ~MockConnectionManager() override;

  // Overridden ServerConnectionManager functions.
  HttpResponse PostBuffer(const std::string& buffer_in,
                          const std::string& access_token,
                          std::string* buffer_out) override;

  // Control of commit response.
  // NOTE: Commit callback is invoked only once then reset.
  void SetMidCommitCallback(base::OnceClosure callback);
  void SetMidCommitObserver(MidCommitObserver* observer);

  // Generic versions of AddUpdate functions. Tests using these function should
  // compile for both the int64_t and string id based versions of the server.
  // The SyncEntity returned is only valid until the Sync is completed
  // (e.g. with SyncShare.) It allows to add further entity properties before
  // sync, using SetLastXXX() methods and/or GetMutableLastUpdate().
  sync_pb::SyncEntity* AddUpdateDirectory(
      const std::string& id,
      const std::string& parent_id,
      const std::string& name,
      int64_t version,
      int64_t sync_ts,
      const std::string& originator_cache_guid,
      const std::string& originator_client_item_id);
  sync_pb::SyncEntity* AddUpdateBookmark(
      const std::string& id,
      const std::string& parent_id,
      const std::string& name,
      int64_t version,
      int64_t sync_ts,
      const std::string& originator_cache_guid,
      const std::string& originator_client_item_id);
  // Versions of the AddUpdate function that accept specifics.
  sync_pb::SyncEntity* AddUpdateSpecifics(
      const std::string& id,
      const std::string& parent_id,
      const std::string& name,
      int64_t version,
      int64_t sync_ts,
      bool is_dir,
      const sync_pb::EntitySpecifics& specifics);
  sync_pb::SyncEntity* AddUpdateSpecifics(
      const std::string& id,
      const std::string& parent_id,
      const std::string& name,
      int64_t version,
      int64_t sync_ts,
      bool is_dir,
      const sync_pb::EntitySpecifics& specifics,
      const std::string& originator_cache_guid,
      const std::string& originator_client_item_id);
  sync_pb::SyncEntity* SetNigori(const std::string& id,
                                 int64_t version,
                                 int64_t sync_ts,
                                 const sync_pb::EntitySpecifics& specifics);
  // Unique client tag variant for adding items.
  sync_pb::SyncEntity* AddUpdatePref(const std::string& id,
                                     const std::string& parent_id,
                                     const std::string& client_tag,
                                     int64_t version,
                                     int64_t sync_ts);

  // Find the last commit sent by the client, and replay it for the next get
  // updates command.  This can be used to simulate the GetUpdates that happens
  // immediately after a successful commit.
  sync_pb::SyncEntity* AddUpdateFromLastCommit();

  // Add a deleted item.  Deletion records typically contain no
  // additional information beyond the deletion, and no specifics.
  // The server may send the originator fields.
  void AddUpdateTombstone(const std::string& id, DataType type);

  void SetLastUpdateDeleted();
  void SetLastUpdateServerTag(const std::string& tag);
  void SetLastUpdateClientTag(const std::string& tag);
  void SetLastUpdateOriginatorFields(const std::string& client_id,
                                     const std::string& entry_id);
  void SetNewTimestamp(int ts);
  void SetChangesRemaining(int64_t count);

  // Add a new batch of updates after the current one.  Allows multiple
  // GetUpdates responses to be buffered up, since the syncer may
  // issue multiple requests during a sync cycle.
  void NextUpdateBatch();

  void FailNextPostBufferToPathCall() { countdown_to_postbuffer_fail_ = 1; }
  void FailNthPostBufferToPathCall(int n) { countdown_to_postbuffer_fail_ = n; }

  void SetKeystoreKey(const std::string& key);

  void FailNonPeriodicGetUpdates() { fail_non_periodic_get_updates_ = true; }

  void SetGUClientCommand(std::unique_ptr<sync_pb::ClientCommand> command);
  void SetCommitClientCommand(std::unique_ptr<sync_pb::ClientCommand> command);

  void SetTransientErrorId(const std::string&);

  const std::vector<std::string>& committed_ids() const {
    return committed_ids_;
  }
  const std::vector<std::unique_ptr<sync_pb::CommitMessage>>& commit_messages()
      const {
    return commit_messages_;
  }
  const std::vector<std::unique_ptr<sync_pb::CommitResponse>>&
  commit_responses() const {
    return commit_responses_;
  }
  // Retrieve the last sent commit message.
  const sync_pb::CommitMessage& last_sent_commit() const;

  // Retrieve the last returned commit response.
  const sync_pb::CommitResponse& last_commit_response() const;

  // Retrieve the last request submitted to the server (regardless of type).
  const sync_pb::ClientToServerMessage& last_request() const;

  // Retrieve the cumulative collection of all requests sent by clients.
  const std::vector<sync_pb::ClientToServerMessage>& requests() const;

  void set_conflict_all_commits(bool value) { conflict_all_commits_ = value; }
  void set_next_new_id(int value) { next_new_id_ = value; }
  void set_conflict_n_commits(int value) { conflict_n_commits_ = value; }

  void set_store_birthday(const std::string& new_birthday) {
    // Multiple threads can set store_birthday_ in our tests, need to lock it to
    // ensure atomic read/writes and avoid race conditions.
    base::AutoLock lock(store_birthday_lock_);
    store_birthday_ = new_birthday;
  }

  void set_throttling(bool value) { throttling_ = value; }

  void set_partial_failure(bool value) { partial_failure_ = value; }

  // Retrieve the number of GetUpdates requests that the mock server has
  // seen since the last time this function was called.  Can be used to
  // verify that a GetUpdates actually did or did not happen after running
  // the syncer.
  int GetAndClearNumGetUpdatesRequests() {
    int result = num_get_updates_requests_;
    num_get_updates_requests_ = 0;
    return result;
  }

  // Expect that GetUpdates will request exactly the types indicated in
  // the bitset.
  void ExpectGetUpdatesRequestTypes(DataTypeSet expected_filter) {
    expected_filter_ = expected_filter;
  }

  // Set partial failure date types.
  void SetPartialFailureTypes(DataTypeSet types) {
    partial_failure_type_ = types;
  }

  void SetServerReachable();

  void SetServerNotReachable();

  // Updates our internal state as if we had attempted a connection.  Does not
  // send notifications as a real connection attempt would.  This is useful in
  // cases where we're mocking out most of the code that performs network
  // requests.
  void UpdateConnectionStatus();

  using ServerConnectionManager::SetServerResponse;

  // Return by copy to be thread-safe.
  const std::string store_birthday() {
    base::AutoLock lock(store_birthday_lock_);
    return store_birthday_;
  }

  // Explicitly indicate that we will not be fetching some updates.
  void ClearUpdatesQueue() { update_queue_.clear(); }

  // Locate the most recent update message for purpose of alteration.
  sync_pb::SyncEntity* GetMutableLastUpdate();

  // Adds a new progress marker to the last update.
  sync_pb::DataTypeProgressMarker* AddUpdateProgressMarker();

  void ResetAccessToken() { ClearAccessToken(); }

 private:
  sync_pb::SyncEntity* AddUpdateFull(const std::string& id,
                                     const std::string& parentid,
                                     const std::string& name,
                                     int64_t version,
                                     int64_t sync_ts,
                                     bool is_dir);
  sync_pb::SyncEntity* AddUpdateMeta(const std::string& id,
                                     const std::string& parentid,
                                     const std::string& name,
                                     int64_t version,
                                     int64_t sync_ts);

  // Functions to handle the various types of server request.
  bool ProcessGetUpdates(sync_pb::ClientToServerMessage* csm,
                         sync_pb::ClientToServerResponse* response);
  bool ProcessCommit(sync_pb::ClientToServerMessage* csm,
                     sync_pb::ClientToServerResponse* response_buffer);
  bool ProcessClearServerData(sync_pb::ClientToServerMessage* csm,
                              sync_pb::ClientToServerResponse* response);
  void AddDefaultBookmarkData(sync_pb::SyncEntity* entity, bool is_folder);

  // Determine if one entry in a commit should be rejected with a conflict.
  bool ShouldConflictThisCommit();

  // Determine if the given item's commit request should be refused with
  // a TRANSIENT_ERROR response.
  bool ShouldTransientErrorThisId(const std::string& id);

  // Get a mutable update response which will eventually be returned to the
  // client.
  sync_pb::GetUpdatesResponse* GetUpdateResponse();
  void ApplyToken();

  // Determine whether an progress marker array (like that sent in
  // GetUpdates.from_progress_marker) indicates that a particular DataType
  // should be included.
  bool IsDataTypePresentInSpecifics(
      const google::protobuf::RepeatedPtrField<sync_pb::DataTypeProgressMarker>&
          filter,
      DataType value);

  sync_pb::DataTypeProgressMarker const* GetProgressMarkerForType(
      const google::protobuf::RepeatedPtrField<sync_pb::DataTypeProgressMarker>&
          filter,
      DataType value);

  // When false, we pretend to have network connectivity issues.
  bool server_reachable_ = true;

  // All IDs that have been committed.
  std::vector<std::string> committed_ids_;

  // List of IDs which should return a transient error.
  std::vector<std::string> transient_error_ids_;

  // Control of when/if we return conflicts.
  bool conflict_all_commits_ = false;
  int conflict_n_commits_ = 0;

  // Commit messages we've sent, and responses we've returned.
  std::vector<std::unique_ptr<sync_pb::CommitMessage>> commit_messages_;
  std::vector<std::unique_ptr<sync_pb::CommitResponse>> commit_responses_;

  // The next id the mock will return to a commit.
  int next_new_id_ = 10000;

  // The store birthday we send to the client.
  std::string store_birthday_ = "Store BDay!";
  base::Lock store_birthday_lock_;
  bool store_birthday_sent_ = false;

  // On each PostBufferToPath() call, we decrement this counter.  The call fails
  // iff we hit zero at that call.
  int countdown_to_postbuffer_fail_ = 0;

  // The updates we'll return to the next request.
  std::list<sync_pb::GetUpdatesResponse> update_queue_;
  base::OnceClosure mid_commit_callback_;
  raw_ptr<MidCommitObserver> mid_commit_observer_ = nullptr;

  // The keystore key we return for a GetUpdates with need_encryption_key set.
  std::string keystore_key_;

  // Whether we are faking a server mandating clients to throttle requests.
  // Protected by |response_code_override_lock_|.
  bool throttling_ = false;

  // Whether we are faking a server mandating clients to partial failure
  // requests.
  // Protected by |response_code_override_lock_|.
  bool partial_failure_ = false;

  base::Lock response_code_override_lock_;

  // True if we are only accepting GetUpdatesCallerInfo::PERIODIC requests.
  bool fail_non_periodic_get_updates_ = false;

  std::unique_ptr<sync_pb::ClientCommand> gu_client_command_;
  std::unique_ptr<sync_pb::ClientCommand> commit_client_command_;

  DataTypeSet expected_filter_;

  DataTypeSet partial_failure_type_;

  int num_get_updates_requests_ = 0;

  std::string next_token_;

  std::vector<sync_pb::ClientToServerMessage> requests_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_CONNECTION_MANAGER_H_
