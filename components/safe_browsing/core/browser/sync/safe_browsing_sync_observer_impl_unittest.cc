// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/safe_browsing_sync_observer_impl.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class SafeBrowsingSyncObserverImplTest : public PlatformTest {
 public:
  SafeBrowsingSyncObserverImplTest() = default;

  void SetUp() override { sync_service_.SetSignedOut(); }

 protected:
  void EnableSync() {
    sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
    sync_service_.FireStateChanged();
  }

  void DisableSync() {
    sync_service_.SetSignedOut();
    sync_service_.FireStateChanged();
  }

  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
};

TEST_F(SafeBrowsingSyncObserverImplTest, ObserveSyncState) {
  SafeBrowsingSyncObserverImpl observer(&sync_service_);
  int invoke_cnt = 0;
  observer.ObserveHistorySyncStateChanged(base::BindRepeating(
      [](int* invoke_cnt) { (*invoke_cnt)++; }, &invoke_cnt));

  EnableSync();
  EXPECT_EQ(invoke_cnt, 1);

  DisableSync();
  EXPECT_EQ(invoke_cnt, 2);

  DisableSync();
  // Not invoked since the state didn't change.
  EXPECT_EQ(invoke_cnt, 2);
}

TEST_F(SafeBrowsingSyncObserverImplTest, NullSyncService) {
  SafeBrowsingSyncObserverImpl observer(nullptr);
  int invoke_cnt = 0;
  observer.ObserveHistorySyncStateChanged(base::BindRepeating(
      [](int* invoke_cnt) { (*invoke_cnt)++; }, &invoke_cnt));

  EnableSync();
  EXPECT_EQ(invoke_cnt, 0);
}

TEST_F(SafeBrowsingSyncObserverImplTest, NullCallback) {
  SafeBrowsingSyncObserverImpl observer(&sync_service_);
  EnableSync();
}

}  // namespace safe_browsing
