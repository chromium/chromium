// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_server.h"

namespace instance_id {
class FakeGCMDriverForInstanceID;
}  // namespace instance_id

namespace fake_server {

// This class is observing changes to the fake server, and sends invalidations
// to clients upon commits. Sent invalidation follows the same format expected
// by the sync invalidations framework (i.e. SyncInvalidationsService).
class FakeServerSyncInvalidationSender : public FakeServer::Observer {
 public:
  // `fake_server` must not be nullptr, and must outlive this object.
  // Persists undelivered invalidations to `storage_dir` across PRE_ tests.
  // When `reflection_blocking_enabled` is true, invalidations caused by a
  // client's own commits (matching `committer_cache_guid`) are suppressed,
  // matching production server reflection blocking.
  FakeServerSyncInvalidationSender(FakeServer* fake_server,
                                   const base::FilePath& storage_dir,
                                   bool reflection_blocking_enabled);
  ~FakeServerSyncInvalidationSender() override;
  FakeServerSyncInvalidationSender(const FakeServerSyncInvalidationSender&) =
      delete;
  FakeServerSyncInvalidationSender& operator=(
      const FakeServerSyncInvalidationSender&) = delete;

  // Add `fake_gcm_driver` to send invalidations to from the fake server.
  void AddFakeGCMDriver(
      instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver);

  // Remove `fake_gcm_driver` to stop sending invalidations.
  void RemoveFakeGCMDriver(
      instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver);

  // Simulates going offline/online for all clients. While offline,
  // invalidations are queued in `invalidations_to_deliver_` and persisted to
  // disk instead of being delivered to handlers. Going back online delivers
  // any queued invalidations immediately.
  void SetOfflineAllClients(bool offline);

  base::WeakPtr<FakeServerSyncInvalidationSender> GetWeakPtr();

  // FakeServer::Observer implementation.
  void OnWillCommit() override;
  void OnCommit(std::string_view committer_cache_guid,
                syncer::DataTypeSet committed_data_types) override;

 private:
  struct SubscribedClientInfo {
    std::string cache_guid;
    syncer::DataTypeSet interested_data_types;
  };

  instance_id::FakeGCMDriverForInstanceID* GetFakeGCMDriverByToken(
      const std::string& fcm_registration_token) const;

  // Delivers all the incoming messages to the corresponding FCM handlers.
  // Messages for FCM tokens which are not registered will be kept.
  void DeliverInvalidationsToHandlers();

  // Updates `token_to_subscribed_client_` from DeviceInfo data type.
  void UpdateSubscribedClientsMap();

  void LoadFakeStateFromDisk();
  void WriteFakeStateToDisk() const;

  const base::raw_ref<FakeServer> fake_server_;
  const base::FilePath fake_state_file_path_;
  const bool reflection_blocking_enabled_;
  bool offline_ = false;

  base::ScopedObservation<FakeServer, FakeServer::Observer>
      fake_server_observation_{this};

  // Cache of invalidations to be dispatched by
  // DeliverInvalidationsToHandlers(), keyed by FCM registration token. If no
  // handler is registered for a token, then the corresponding invalidations
  // will remain here until a handler is added.
  base::flat_map<std::string, std::vector<sync_pb::SyncInvalidationsPayload>>
      invalidations_to_deliver_;

  // Map from FCM registration token to its subscribed client info.
  base::flat_map<std::string, SubscribedClientInfo> token_to_subscribed_client_;

  std::vector<raw_ptr<instance_id::FakeGCMDriverForInstanceID>>
      fake_gcm_drivers_;

  base::WeakPtrFactory<FakeServerSyncInvalidationSender> weak_ptr_factory_{
      this};
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
