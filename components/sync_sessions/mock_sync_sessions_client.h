// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_MOCK_SYNC_SESSIONS_CLIENT_H_
#define COMPONENTS_SYNC_SESSIONS_MOCK_SYNC_SESSIONS_CLIENT_H_

#include <string>

#include "components/sync_sessions/sync_sessions_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace sync_sessions {

class MockSyncSessionsClient : public SyncSessionsClient {
 public:
  // By default, ShouldSyncURL() always returns true.
  MockSyncSessionsClient();
  ~MockSyncSessionsClient() override;
  MOCK_METHOD(SessionSyncPrefs*, GetSessionSyncPrefs, (), (override));
  MOCK_METHOD(syncer::RepeatingModelTypeStoreFactory,
              GetStoreFactory,
              (),
              (override));
  MOCK_METHOD(void, ClearAllOnDemandFavicons, (), (override));
  MOCK_METHOD(bool, ShouldSyncURL, (const GURL& url), (const override));
  MOCK_METHOD(bool,
              IsRecentLocalCacheGuid,
              (const std::string& cache_guid),
              (const override));
  MOCK_METHOD(SyncedWindowDelegatesGetter*,
              GetSyncedWindowDelegatesGetter,
              (),
              (override));
  MOCK_METHOD(LocalSessionEventRouter*,
              GetLocalSessionEventRouter,
              (),
              (override));
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_MOCK_SYNC_SESSIONS_CLIENT_H_
