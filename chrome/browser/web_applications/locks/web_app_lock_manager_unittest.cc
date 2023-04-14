// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

class WebAppLockManagerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void FlushTaskRunner() {
    base::test::TestFuture<void> future;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }
};

TEST_F(WebAppLockManagerTest, DebugValueNoCrash) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppLockManager& lock_manager = provider->command_manager().lock_manager();

  base::test::TestFuture<std::unique_ptr<AppLock>> lock1_future;
  lock_manager.AcquireLock(AppLockDescription("abc"),
                           lock1_future.GetCallback(), FROM_HERE);

  base::test::TestFuture<std::unique_ptr<AllAppsLock>> lock2_future;
  lock_manager.AcquireLock(AllAppsLockDescription(), lock2_future.GetCallback(),
                           FROM_HERE);

  base::test::TestFuture<std::unique_ptr<AppLock>> lock3_future;
  lock_manager.AcquireLock(AppLockDescription("abc"),
                           lock3_future.GetCallback(), FROM_HERE);

  ASSERT_TRUE(lock1_future.Wait());

  base::Value debug_value = lock_manager.ToDebugValue();
  ASSERT_TRUE(debug_value.is_dict());
}

TEST_F(WebAppLockManagerTest, AllAppsLock) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppLockManager& lock_manager = provider->command_manager().lock_manager();

  // - AppLock blocks AllAppsLock
  // - AllAppsLock does not block NoopLock
  // - AllAppsLock blocks AppLock

  base::test::TestFuture<std::unique_ptr<AppLock>> app_future;
  lock_manager.AcquireLock(AppLockDescription("abc"), app_future.GetCallback(),
                           FROM_HERE);

  base::test::TestFuture<std::unique_ptr<AllAppsLock>> all_apps_future;
  lock_manager.AcquireLock(AllAppsLockDescription(),
                           all_apps_future.GetCallback(), FROM_HERE);

  base::test::TestFuture<std::unique_ptr<NoopLock>> noop_lock_future;
  lock_manager.AcquireLock(NoopLockDescription(),
                           noop_lock_future.GetCallback(), FROM_HERE);

  base::test::TestFuture<std::unique_ptr<AppLock>> app_future2;
  lock_manager.AcquireLock(AppLockDescription("abc"), app_future2.GetCallback(),
                           FROM_HERE);

  // Wait for the first AppLock & NoopLock.
  ASSERT_TRUE(app_future.Wait());
  ASSERT_TRUE(noop_lock_future.Wait());
  FlushTaskRunner();
  // Verify other locks are not granted.
  EXPECT_FALSE(all_apps_future.IsReady());
  EXPECT_FALSE(app_future2.IsReady());

  // Release the first lock, allowing AllAppsLock to be granted.
  app_future.Take().reset();
  FlushTaskRunner();
  ASSERT_TRUE(all_apps_future.Wait());
  // Verify AppLock is not granted.
  FlushTaskRunner();
  EXPECT_FALSE(app_future2.IsReady());

  // Release the AllAppsLock, verify AppLock is granted.
  all_apps_future.Take().reset();
  ASSERT_TRUE(app_future2.Wait());
}

}  // namespace web_app
