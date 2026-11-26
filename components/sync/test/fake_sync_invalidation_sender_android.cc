// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_invalidation_sender_android.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"

namespace fake_server {

FakeSyncInvalidationSenderAndroid::FakeSyncInvalidationSenderAndroid() =
    default;

FakeSyncInvalidationSenderAndroid::~FakeSyncInvalidationSenderAndroid() =
    default;

void FakeSyncInvalidationSenderAndroid::SetOfflineAllClients(bool offline) {
  offline_ = offline;
  if (!offline_) {
    DeliverInvalidationsToHandlers();
  }
}

void FakeSyncInvalidationSenderAndroid::SendInvalidations(
    base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
        token_to_payload) {
  for (auto& [token, payload] : token_to_payload) {
    invalidations_to_deliver_[token].push_back(std::move(payload));
  }
  DeliverInvalidationsToHandlers();
}

void FakeSyncInvalidationSenderAndroid::DeliverInvalidationsToHandlers() {
  if (offline_) {
    DVLOG(1) << "Not delivering invalidations while network is offline.";
    return;
  }

  DVLOG(1) << "Trying to deliver invalidations for "
           << invalidations_to_deliver_.size() << " FCM tokens.";
  base::EraseIf(invalidations_to_deliver_, [this](const auto& entry) {
    const auto& [token, invalidations] = entry;
    for (const sync_pb::SyncInvalidationsPayload& payload : invalidations) {
      if (!fake_instance_id_android_.DispatchMessageForToken(
              token, payload.SerializeAsString())) {
        DVLOG(1) << "Could not dispatch invalidation for token: " << token;
        return false;
      }
    }
    return true;
  });
}

}  // namespace fake_server
