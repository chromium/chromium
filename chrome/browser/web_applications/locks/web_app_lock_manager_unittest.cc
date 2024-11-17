// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
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

  WebAppLockManager& lock_manager() {
    WebAppProvider* provider = WebAppProvider::GetForTest(profile());
    return provider->command_manager().lock_manager();
  }

  void FlushTaskRunner() {
    base::test::TestFuture<void> future;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }
};

TEST_F(WebAppLockManagerTest, NoopLock) {
  std::unique_ptr<NoopLock> lock1 = std::make_unique<NoopLock>();
  EXPECT_FALSE(lock1->IsGranted());
  base::test::TestFuture<void> lock1_future;
  lock_manager().AcquireLock(NoopLockDescription(), *lock1,
                             lock1_future.GetCallback(), FROM_HERE);

  std::unique_ptr<NoopLock> lock2 = std::make_unique<NoopLock>();
  EXPECT_FALSE(lock2->IsGranted());
  base::test::TestFuture<void> lock2_future;
  lock_manager().AcquireLock(NoopLockDescription(), *lock2,
                             lock2_future.GetCallback(), FROM_HERE);

  // Ensure async.
  EXPECT_FALSE(lock1->IsGranted());
  EXPECT_FALSE(lock2->IsGranted());

  // Locks should not block each other.
  ASSERT_TRUE(lock1_future.Wait());
  ASSERT_TRUE(lock2_future.Wait());
  EXPECT_TRUE(lock1->IsGranted());
  EXPECT_TRUE(lock2->IsGranted());
}

TEST_F(WebAppLockManagerTest, AppLock) {
  std::unique_ptr<AppLock> lock1 = std::make_unique<AppLock>();
  EXPECT_FALSE(lock1->IsGranted());
  base::test::TestFuture<void> lock1_future;
  lock_manager().AcquireLock(AppLockDescription("abc"), *lock1,
                             lock1_future.GetCallback(), FROM_HERE);

  std::unique_ptr<AppLock> lock2 = std::make_unique<AppLock>();
  EXPECT_FALSE(lock2->IsGranted());
  base::test::TestFuture<void> lock2_future;
  lock_manager().AcquireLock(AppLockDescription("abc"), *lock2,
                             lock2_future.GetCallback(), FROM_HERE);

  // Ensure async.
  EXPECT_FALSE(lock1->IsGranted());

  // Lock 1 grants. Lock 2 should be blocked.
  ASSERT_TRUE(lock1_future.Wait());
  EXPECT_TRUE(lock1->IsGranted());
  EXPECT_FALSE(lock2->IsGranted());
  // Ensure blocked
  FlushTaskRunner();
  EXPECT_FALSE(lock2->IsGranted());

  // Ensure async again.
  lock1.reset();
  EXPECT_FALSE(lock2->IsGranted());

  // Lock 2 can grant.
  ASSERT_TRUE(lock2_future.Wait());
  EXPECT_TRUE(lock2->IsGranted());
}

TEST_F(WebAppLockManagerTest, DebugValueNoCrash) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppLockManager& lock_manager = provider->command_manager().lock_manager();

  std::unique_ptr<AppLock> lock1 = std::make_unique<AppLock>();
  base::test::TestFuture<void> lock1_future;
  lock_manager.AcquireLock(AppLockDescription("abc"), *lock1,
                           lock1_future.GetCallback(), FROM_HERE);

  std::unique_ptr<AllAppsLock> lock2 = std::make_unique<AllAppsLock>();
  base::test::TestFuture<void> lock2_future;
  lock_manager.AcquireLock(AllAppsLockDescription(), *lock2,
                           lock2_future.GetCallback(), FROM_HERE);

  std::unique_ptr<AppLock> lock3 = std::make_unique<AppLock>();
  base::test::TestFuture<void> lock3_future;
  lock_manager.AcquireLock(AppLockDescription("abc"), *lock3,
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

  std::unique_ptr<AppLock> app_lock = std::make_unique<AppLock>();
  base::test::TestFuture<void> app_future;
  lock_manager.AcquireLock(AppLockDescription("abc"), *app_lock,
                           app_future.GetCallback(), FROM_HERE);

  std::unique_ptr<AllAppsLock> all_apps_lock = std::make_unique<AllAppsLock>();
  base::test::TestFuture<void> all_apps_future;
  lock_manager.AcquireLock(AllAppsLockDescription(), *all_apps_lock,
                           all_apps_future.GetCallback(), FROM_HERE);

  std::unique_ptr<NoopLock> noop_lock = std::make_unique<NoopLock>();
  base::test::TestFuture<void> noop_lock_future;
  lock_manager.AcquireLock(NoopLockDescription(), *noop_lock,
                           noop_lock_future.GetCallback(), FROM_HERE);

  std::unique_ptr<AppLock> app_lock2 = std::make_unique<AppLock>();
  base::test::TestFuture<void> app_future2;
  lock_manager.AcquireLock(AppLockDescription("abc"), *app_lock2,
                           app_future2.GetCallback(), FROM_HERE);

  // Wait for the first AppLock & NoopLock.
  ASSERT_TRUE(app_future.Wait());
  ASSERT_TRUE(noop_lock_future.Wait());
  FlushTaskRunner();
  // Verify other locks are not granted.
  EXPECT_FALSE(all_apps_future.IsReady());
  EXPECT_FALSE(app_future2.IsReady());

  // Release the first lock, allowing AllAppsLock to be granted.
  app_lock.reset();
  FlushTaskRunner();
  ASSERT_TRUE(all_apps_future.Wait());
  // Verify AppLock is not granted.
  FlushTaskRunner();
  EXPECT_FALSE(app_future2.IsReady());

  // Release the AllAppsLock, verify AppLock is granted.
  all_apps_lock.reset();
  ASSERT_TRUE(app_future2.Wait());
}

TEST_F(WebAppLockManagerTest, AllAppsLockBlocksUpgrade) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppLockManager& lock_manager = provider->command_manager().lock_manager();

  // - AppLock blocks AllAppsLock
  // - AllAppsLock does not block NoopLock
  // - AllAppsLock blocks AppLock

  std::unique_ptr<AllAppsLock> all_apps_lock = std::make_unique<AllAppsLock>();
  base::test::TestFuture<void> all_apps_lock_future;
  lock_manager.AcquireLock(AllAppsLockDescription(), *all_apps_lock,
                           all_apps_lock_future.GetCallback(), FROM_HERE);

  std::unique_ptr<NoopLock> noop_lock = std::make_unique<NoopLock>();
  base::test::TestFuture<void> noop_lock_future;
  lock_manager.AcquireLock(NoopLockDescription(), *noop_lock,
                           noop_lock_future.GetCallback(), FROM_HERE);

  std::unique_ptr<SharedWebContentsLock> web_contents_lock =
      std::make_unique<SharedWebContentsLock>();
  base::test::TestFuture<void> web_contents_lock_future;
  lock_manager.AcquireLock(SharedWebContentsLockDescription(),
                           *web_contents_lock,
                           web_contents_lock_future.GetCallback(), FROM_HERE);

  // Wait for all locks to acquire.
  EXPECT_FALSE(all_apps_lock->IsGranted());
  ASSERT_TRUE(all_apps_lock_future.Wait());
  EXPECT_TRUE(all_apps_lock->IsGranted());

  ASSERT_TRUE(noop_lock_future.Wait());
  EXPECT_TRUE(noop_lock->IsGranted());
  ASSERT_TRUE(web_contents_lock_future.Wait());
  EXPECT_TRUE(web_contents_lock->IsGranted());

  // Upgrade both locks.
  std::unique_ptr<AppLock> app_lock = std::make_unique<AppLock>();
  base::test::TestFuture<void> app_lock_future;
  lock_manager.UpgradeAndAcquireLock(std::move(noop_lock), *app_lock, {"a"},
                                     app_lock_future.GetCallback(), FROM_HERE);

  std::unique_ptr<SharedWebContentsWithAppLock> web_contents_with_app_lock =
      std::make_unique<SharedWebContentsWithAppLock>();
  base::test::TestFuture<void> web_contents_with_app_lock_future;
  lock_manager.UpgradeAndAcquireLock(
      std::move(web_contents_lock), *web_contents_with_app_lock, {"b"},
      web_contents_with_app_lock_future.GetCallback(), FROM_HERE);

  // Verify upgrades are not granted.
  FlushTaskRunner();
  EXPECT_FALSE(app_lock_future.IsReady());
  EXPECT_FALSE(app_lock->IsGranted());
  EXPECT_FALSE(web_contents_with_app_lock_future.IsReady());
  EXPECT_FALSE(web_contents_with_app_lock->IsGranted());

  // Release the all apps lock, verify the new locks are granted.
  all_apps_lock.reset();
  EXPECT_TRUE(app_lock_future.Wait());
  EXPECT_TRUE(app_lock->IsGranted());
  EXPECT_TRUE(web_contents_with_app_lock_future.Wait());
  EXPECT_TRUE(web_contents_with_app_lock->IsGranted());
}

TEST_F(WebAppLockManagerTest, UpgradeInheritsLocks) {
  std::unique_ptr<SharedWebContentsLock> web_contents_lock =
      std::make_unique<SharedWebContentsLock>();
  base::test::TestFuture<void> web_contents_lock_future;
  lock_manager().AcquireLock(SharedWebContentsLockDescription(),
                             *web_contents_lock,
                             web_contents_lock_future.GetCallback(), FROM_HERE);
  ASSERT_TRUE(web_contents_lock_future.Wait());
  EXPECT_TRUE(web_contents_lock->IsGranted());

  // Upgrade
  std::unique_ptr<SharedWebContentsWithAppLock> web_contents_with_app_lock =
      std::make_unique<SharedWebContentsWithAppLock>();
  base::test::TestFuture<void> web_contents_with_app_lock_future;
  lock_manager().UpgradeAndAcquireLock(
      std::move(web_contents_lock), *web_contents_with_app_lock, {"b"},
      web_contents_with_app_lock_future.GetCallback(), FROM_HERE);

  // Check that the old web contents lock is still locked.
  std::unique_ptr<SharedWebContentsLock> web_contents_lock2 =
      std::make_unique<SharedWebContentsLock>();
  base::test::TestFuture<void> web_contents_lock2_future;
  lock_manager().AcquireLock(
      SharedWebContentsLockDescription(), *web_contents_lock2,
      web_contents_lock2_future.GetCallback(), FROM_HERE);

  FlushTaskRunner();
  EXPECT_FALSE(web_contents_lock2->IsGranted());

  // Releasing the upgraded lock now allows the other to grant.
  web_contents_with_app_lock.reset();
  ASSERT_TRUE(web_contents_lock2_future.Wait());
  EXPECT_TRUE(web_contents_lock2->IsGranted());
}

}  // namespace web_app
