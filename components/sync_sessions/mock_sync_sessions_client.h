// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_MOCK_SYNC_SESSIONS_CLIENT_H_
#define COMPONENTS_SYNC_SESSIONS_MOCK_SYNC_SESSIONS_CLIENT_H_

#include "components/sync_sessions/sync_sessions_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace sync_sessions {

class MockSyncSessionsClient : public SyncSessionsClient {
 public:
  // By default, ShouldSyncURL() always returns true.
  MockSyncSessionsClient();
  ~MockSyncSessionsClient() override;

  MOCK_METHOD0(GetFaviconService, favicon::FaviconService*());
  MOCK_METHOD0(GetHistoryService, history::HistoryService*());
  MOCK_METHOD0(GetSessionSyncPrefs, SessionSyncPrefs*());
  MOCK_METHOD0(GetStoreFactory, syncer::RepeatingModelTypeStoreFactory());
  MOCK_CONST_METHOD1(ShouldSyncURL, bool(const GURL& url));
  MOCK_METHOD0(GetSyncedWindowDelegatesGetter, SyncedWindowDelegatesGetter*());
  MOCK_METHOD0(GetLocalSessionEventRouter, LocalSessionEventRouter*());
  MOCK_METHOD0(IsProxyTabsSyncRunning, bool());
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_MOCK_SYNC_SESSIONS_CLIENT_H_
