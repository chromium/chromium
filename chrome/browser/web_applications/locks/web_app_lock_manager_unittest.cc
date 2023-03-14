// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
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
};

TEST_F(WebAppLockManagerTest, DebugValueNoCrash) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppLockManager& lock_manager = provider->command_manager().lock_manager();

  base::test::TestFuture<std::unique_ptr<AppLock>> lock1_future;
  lock_manager.AcquireLock(AppLockDescription("abc"),
                           lock1_future.GetCallback(), FROM_HERE);

  base::test::TestFuture<std::unique_ptr<FullSystemLock>> lock2_future;
  lock_manager.AcquireLock(FullSystemLockDescription(),
                           lock2_future.GetCallback(), FROM_HERE);

  base::test::TestFuture<std::unique_ptr<AppLock>> lock3_future;
  lock_manager.AcquireLock(AppLockDescription("abc"),
                           lock3_future.GetCallback(), FROM_HERE);

  ASSERT_TRUE(lock1_future.Wait());

  base::Value debug_value = lock_manager.ToDebugValue();
  ASSERT_TRUE(debug_value.is_dict());
}

}  // namespace web_app
