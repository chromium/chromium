// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_GCM_DRIVER_SYNC_INVALIDATION_SENDER_H_
#define COMPONENTS_SYNC_TEST_FAKE_GCM_DRIVER_SYNC_INVALIDATION_SENDER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_sync_invalidation_sender.h"

namespace instance_id {
class FakeGCMDriverForInstanceID;
}  // namespace instance_id

namespace fake_server {

// `FakeSyncInvalidationSender` implementation that dispatches sync
// invalidations in-process to registered `FakeGCMDriverForInstanceID`
// instances.
class FakeGCMDriverSyncInvalidationSender : public FakeSyncInvalidationSender {
 public:
  FakeGCMDriverSyncInvalidationSender();
  ~FakeGCMDriverSyncInvalidationSender() override;

  // FakeSyncInvalidationSender:
  void Start(FakeServer* fake_server,
             const base::FilePath& storage_dir) override;
  void SetOfflineAllClients(bool offline) override;

  // Adds `fake_gcm_driver` (which must not be null and must outlive the
  // registration or be removed via `RemoveFakeGCMDriver()`) to receive
  // invalidations from the fake server.
  void AddFakeGCMDriver(
      instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver);

  // Removes `fake_gcm_driver` to stop sending invalidations.
  void RemoveFakeGCMDriver(
      instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver);

  base::WeakPtr<FakeGCMDriverSyncInvalidationSender> GetWeakPtr();

 protected:
  // FakeSyncInvalidationSender:
  void SendInvalidations(
      base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
          token_to_payload) override;

 private:
  instance_id::FakeGCMDriverForInstanceID* GetFakeGCMDriverByToken(
      const std::string& fcm_registration_token);

  // Delivers all the incoming messages to the corresponding FCM handlers.
  // Messages for FCM tokens which are not registered will be kept.
  void DeliverInvalidationsToHandlers();

  void LoadFakeStateFromDisk();
  void WriteFakeStateToDisk() const;

  base::FilePath fake_state_file_path_;
  bool offline_ = false;

  // Cache of invalidations to be dispatched by
  // `DeliverInvalidationsToHandlers()`, keyed by FCM registration token. If no
  // handler is registered for a token, then the corresponding invalidations
  // will remain here until a handler is added.
  base::flat_map<std::string, std::vector<sync_pb::SyncInvalidationsPayload>>
      invalidations_to_deliver_;

  std::vector<raw_ptr<instance_id::FakeGCMDriverForInstanceID>>
      fake_gcm_drivers_;

  base::WeakPtrFactory<FakeGCMDriverSyncInvalidationSender> weak_ptr_factory_{
      this};
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_GCM_DRIVER_SYNC_INVALIDATION_SENDER_H_
