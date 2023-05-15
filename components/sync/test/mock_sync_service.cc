// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_sync_service.h"

namespace syncer {

MockSyncService::MockSyncService() {
  // A sensible default is to return true, because in most cases it is used in
  // combination with HasSyncConsent(), which defaults to false.
  ON_CALL(*this, IsSyncFeatureConsideredRequested)
      .WillByDefault(testing::Return(true));
}

MockSyncService::~MockSyncService() = default;

syncer::SyncUserSettingsMock* MockSyncService::GetMockUserSettings() {
  return &user_settings_;
}

syncer::SyncUserSettings* MockSyncService::GetUserSettings() {
  return &user_settings_;
}

const syncer::SyncUserSettings* MockSyncService::GetUserSettings() const {
  return &user_settings_;
}

}  // namespace syncer
