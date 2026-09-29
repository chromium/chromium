// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/device_ownership_waiter_impl.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_manager {

class DeviceOwnershipWaiterTest : public testing::Test {
 public:
  DeviceOwnershipWaiterTest() = default;

  ~DeviceOwnershipWaiterTest() override = default;

  void SetUp() override {
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
  }

  void TearDown() override { user_session_test_environment_.reset(); }

  void SetOwnerId(const AccountId& id) { UserManager::Get()->SetOwnerId(id); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> stub_install_attributes_{
      std::make_unique<ash::ScopedStubInstallAttributes>(
          ash::StubInstallAttributes::CreateUnset())};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

TEST_F(DeviceOwnershipWaiterTest, DelaysCorrectly) {
  // Since we skip the actual delay for ownership if we're running in a ChromeOS
  // on Linux build, we mock that behavior by pretending to be ChromeOS.
  const char kLsbReleaseValidChromeOs[] = "CHROMEOS_RELEASE_NAME=Chrome OS\n";

  base::test::ScopedChromeOSVersionInfo version(kLsbReleaseValidChromeOs,
                                                base::Time());
  DeviceOwnershipWaiterImpl waiter;

  base::test::TestFuture<void> future;
  waiter.WaitForOwnershipFetched(future.GetCallback());

  SetOwnerId(user_manager::StubAccountId());

  EXPECT_TRUE(future.Wait());
}

// Tests that on a ChromeOS on Linux build, the delay
// is skipped and instead the callback invoked immediately.
TEST_F(DeviceOwnershipWaiterTest, DoesNotDelayForChromeOsOnLinux) {
  const char kLsbReleaseNonChromeOs[] = "CHROMEOS_RELEASE_NAME=Non Chrome OS\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbReleaseNonChromeOs,
                                                base::Time());

  DeviceOwnershipWaiterImpl waiter;

  base::test::TestFuture<void> future;
  waiter.WaitForOwnershipFetched(future.GetCallback());

  EXPECT_TRUE(future.Wait());
}

}  // namespace user_manager
