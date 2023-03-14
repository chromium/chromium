// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_medium.h"
#include "chrome/services/sharing/nearby/platform/count_down_latch.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class BleV2MediumTest : public testing::Test {
 public:
  BleV2MediumTest() = default;
  ~BleV2MediumTest() override = default;
  BleV2MediumTest(const BleV2MediumTest&) = delete;
  BleV2MediumTest& operator=(const BleV2MediumTest&) = delete;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BleV2MediumTest, TestScanning) {
  auto ble_v2_medium = std::make_unique<BleV2Medium>();
  CountDownLatch scanning_started_latch(1);
  CountDownLatch found_advertisement_latch(1);
  api::ble_v2::BleMedium::ScanningCallback scanning_callback = {
      .start_scanning_result =
          [&scanning_started_latch](absl::Status status) {
            scanning_started_latch.CountDown();
          },
      .advertisement_found_cb =
          [&found_advertisement_latch](
              api::ble_v2::BlePeripheral& peripheral,
              const api::ble_v2::BleAdvertisementData& advertisement_data) {
            found_advertisement_latch.CountDown();
          }};

  auto scanning_session = ble_v2_medium->StartScanning(
      /*service_uuid=*/{}, /*tx_power_level=*/{}, std::move(scanning_callback));

  EXPECT_TRUE(scanning_started_latch.Await().Ok());
  EXPECT_TRUE(found_advertisement_latch.Await().Ok());
  auto status = scanning_session->stop_scanning();
  EXPECT_TRUE(status.ok());
}

}  // namespace nearby::chrome
