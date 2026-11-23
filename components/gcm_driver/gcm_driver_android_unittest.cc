// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_android.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/gcm_driver/gcm_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

constexpr char kTestAppId[] = "test_app_id";
constexpr char kTestSenderId[] = "test_sender_id";

class GCMDriverAndroidTest : public testing::Test {
 protected:
  std::unique_ptr<GCMDriverAndroid> CreateDriver(bool enabled) {
    return std::make_unique<GCMDriverAndroid>(
        base::FilePath(), /*blocking_task_runner=*/nullptr, enabled);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(GCMDriverAndroidTest, EnabledAndDisabledCanCoexist) {
  std::unique_ptr<GCMDriverAndroid> enabled_driver =
      CreateDriver(/*enabled=*/true);
  std::unique_ptr<GCMDriverAndroid> disabled_driver =
      CreateDriver(/*enabled=*/false);

  // Destroying the disabled driver must not tear down the Java GCMDriver
  // singleton owned by the enabled driver.
  disabled_driver->Shutdown();
  disabled_driver.reset();

  enabled_driver->Shutdown();
  enabled_driver.reset();
}

TEST_F(GCMDriverAndroidTest, DisabledRegisterReturnsGcmDisabled) {
  std::unique_ptr<GCMDriverAndroid> disabled_driver =
      CreateDriver(/*enabled=*/false);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  disabled_driver->Register(kTestAppId, {kTestSenderId}, future.GetCallback());

  auto [registration_id, result] = future.Take();
  EXPECT_TRUE(registration_id.empty());
  EXPECT_EQ(GCMClient::GCM_DISABLED, result);

  disabled_driver->Shutdown();
}

}  // namespace
}  // namespace gcm
