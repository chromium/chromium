// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/local_session_event_router.h"

namespace sync_sessions {

MockSyncSessionsClient::MockSyncSessionsClient() {
  ON_CALL(*this, ShouldSyncURL(testing::_))
      .WillByDefault(testing::Return(true));
}

MockSyncSessionsClient::~MockSyncSessionsClient() = default;

base::WeakPtr<SyncSessionsClient> MockSyncSessionsClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace sync_sessions
