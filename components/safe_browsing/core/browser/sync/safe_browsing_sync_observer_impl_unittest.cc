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
  void SignIn() {
    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
    sync_service_.FireStateChanged();
  }

  void SignOut() {
    sync_service_.SetSignedOut();
    sync_service_.FireStateChanged();
  }

  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
};

TEST_F(SafeBrowsingSyncObserverImplTest, ObserveSyncState) {
  SafeBrowsingSyncObserverImpl observer(&sync_service_);
  int invoke_count = 0;
  observer.ObserveHistorySyncStateChanged(base::BindRepeating(
      [](int* invoke_count) { (*invoke_count)++; }, &invoke_count));

  SignIn();
  EXPECT_EQ(invoke_count, 1);

  SignOut();
  EXPECT_EQ(invoke_count, 2);

  SignOut();
  // Not invoked since the state didn't change.
  EXPECT_EQ(invoke_count, 2);
}

TEST_F(SafeBrowsingSyncObserverImplTest, NullSyncService) {
  SafeBrowsingSyncObserverImpl observer(nullptr);
  int invoke_count = 0;
  observer.ObserveHistorySyncStateChanged(base::BindRepeating(
      [](int* invoke_count) { (*invoke_count)++; }, &invoke_count));

  SignIn();
  EXPECT_EQ(invoke_count, 0);
}

TEST_F(SafeBrowsingSyncObserverImplTest, NullCallback) {
  SafeBrowsingSyncObserverImpl observer(&sync_service_);
  SignIn();
}

}  // namespace safe_browsing
