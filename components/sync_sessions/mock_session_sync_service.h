// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_MOCK_SESSION_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_SESSIONS_MOCK_SESSION_SYNC_SERVICE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/session_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_sessions {

class MockSessionSyncService : public SessionSyncService {
 public:
  MockSessionSyncService();

  MockSessionSyncService(const MockSessionSyncService&) = delete;
  MockSessionSyncService& operator=(const MockSessionSyncService&) = delete;

  ~MockSessionSyncService() override;

  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(syncer::GlobalIdMapper*,
              GetGlobalIdMapper,
              (),
              (const, override));
  MOCK_METHOD(OpenTabsUIDelegate*, GetOpenTabsUIDelegate, (), (override));
  MOCK_METHOD(void,
              AddTabScreenshot,
              (SessionID tab_id,
               std::string&& screenshot_data,
               const GURL& url),
              (override));
  MOCK_METHOD(void,
              ReadTabScreenshot,
              (const std::string& session_tag,
               SessionID tab_id,
               base::OnceCallback<void(std::optional<std::string>)> callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              SubscribeToForeignSessionsChanged,
              (const base::RepeatingClosure& cb),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              (),
              (override));
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_MOCK_SESSION_SYNC_SERVICE_H_
