// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auto_sign_out/auto_sign_out_service.h"

#include <memory>

#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AutoSignOutTest : public testing::Test {
 public:
  AutoSignOutTest() = default;
  ~AutoSignOutTest() override = default;

 protected:
  syncer::FakeDeviceInfoSyncService* fake_device_info_sync_service() {
    return fake_device_info_sync_service_.get();
  }

  void SetUp() override {
    fake_device_info_sync_service_ =
        std::make_unique<syncer::FakeDeviceInfoSyncService>(
            /*skip_engine_connection=*/true);
  }

  void TearDown() override { fake_device_info_sync_service_.reset(); }

 private:
  std::unique_ptr<syncer::FakeDeviceInfoSyncService>
      fake_device_info_sync_service_;
};

// Verifies that local device timestamp is updated when service is created.
TEST_F(AutoSignOutTest, TimestampUpdatedAfterServiceCreation) {
  const syncer::DeviceInfo* const local_device_info =
      fake_device_info_sync_service()
          ->GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();

  // Ensure local_device is initialized with fake info.
  ASSERT_NE(nullptr, local_device_info);

  EXPECT_FALSE(local_device_info->floating_workspace_last_signin_timestamp()
                   .has_value());

  AutoSignOutService auto_sign_out_service(fake_device_info_sync_service());

  EXPECT_TRUE(local_device_info->floating_workspace_last_signin_timestamp()
                  .has_value());
}

}  // namespace ash
