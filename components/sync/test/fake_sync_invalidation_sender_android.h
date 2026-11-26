// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_ANDROID_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_ANDROID_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_sync_invalidation_sender.h"

namespace fake_server {

// `FakeSyncInvalidationSender` implementation for Android that owns a
// `ScopedUseFakeInstanceIDAndroid` and dispatches sync invalidations to
// `GCMDriver` via `FakeInstanceIDWithSubtype`.
class FakeSyncInvalidationSenderAndroid : public FakeSyncInvalidationSender {
 public:
  FakeSyncInvalidationSenderAndroid();
  ~FakeSyncInvalidationSenderAndroid() override;

  // FakeSyncInvalidationSender:
  void SetOfflineAllClients(bool offline) override;

 protected:
  // FakeSyncInvalidationSender:
  void SendInvalidations(
      base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
          token_to_payload) override;

 private:
  void DeliverInvalidationsToHandlers();

  instance_id::ScopedUseFakeInstanceIDAndroid fake_instance_id_android_;
  bool offline_ = false;

  // Cache of invalidations to be dispatched by
  // `DeliverInvalidationsToHandlers()`, keyed by FCM registration token.
  base::flat_map<std::string, std::vector<sync_pb::SyncInvalidationsPayload>>
      invalidations_to_deliver_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_INVALIDATION_SENDER_ANDROID_H_
