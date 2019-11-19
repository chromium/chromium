// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace chromeos {
namespace cros_healthd {
namespace {

base::Optional<std::vector<mojom::NonRemovableBlockDeviceInfoPtr>>
MakeNonRemovableBlockDeviceInfo() {
  std::vector<mojom::NonRemovableBlockDeviceInfoPtr> info;
  info.push_back(mojom::NonRemovableBlockDeviceInfo::New(
      "test_path", 123 /* size */, "test_type", 10 /* manfid */, "test_name",
      768 /* serial */));
  info.push_back(mojom::NonRemovableBlockDeviceInfo::New(
      "test_path2", 124 /* size */, "test_type2", 11 /* manfid */, "test_name2",
      767 /* serial */));
  return base::Optional<std::vector<mojom::NonRemovableBlockDeviceInfoPtr>>(
      std::move(info));
}

mojom::BatteryInfoPtr MakeBatteryInfo() {
  return mojom::BatteryInfo::New(
      2 /* cycle_count */, 12.9 /* voltage_now */,
      "battery_vendor" /* vendor */, "serial_number" /* serial_number */,
      5.275 /* charge_full_design */, 5.292 /* charge_full */,
      11.55 /* voltage_min_design */, 51785890 /* manufacture_date_smart */,
      /*temperature smart=*/981729, /*model_name=*/"battery_model",
      /*charge_now=*/5.123);
}

mojom::CachedVpdInfoPtr MakeCachedVpdInfo() {
  return mojom::CachedVpdInfo::New("fake_sku_number" /* sku_number */);
}

mojom::TelemetryInfoPtr MakeTelemetryInfo() {
  return mojom::TelemetryInfo::New(
      MakeBatteryInfo() /* battery_info */,
      MakeNonRemovableBlockDeviceInfo() /* block_device_info */,
      MakeCachedVpdInfo() /* vpd_info */
  );
}

class CrosHealthdServiceConnectionTest : public testing::Test {
 public:
  CrosHealthdServiceConnectionTest() = default;

  void SetUp() override { CrosHealthdClient::InitializeFake(); }

  void TearDown() override { CrosHealthdClient::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(CrosHealthdServiceConnectionTest);
};

TEST_F(CrosHealthdServiceConnectionTest, ProbeTelemetryInfo) {
  // Test that we can send a request without categories.
  auto empty_info = mojom::TelemetryInfo::New();
  FakeCrosHealthdClient::Get()->SetProbeTelemetryInfoResponseForTesting(
      empty_info);
  const std::vector<mojom::ProbeCategoryEnum> no_categories = {};
  bool callback_done = false;
  ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      no_categories, base::BindOnce(
                         [](bool* callback_done, mojom::TelemetryInfoPtr info) {
                           EXPECT_EQ(info, mojom::TelemetryInfo::New());
                           *callback_done = true;
                         },
                         &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);

  // Test that we can request all categories.
  auto response_info = MakeTelemetryInfo();
  FakeCrosHealthdClient::Get()->SetProbeTelemetryInfoResponseForTesting(
      response_info);
  const std::vector<mojom::ProbeCategoryEnum> categories_to_test = {
      mojom::ProbeCategoryEnum::kBattery,
      mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
      mojom::ProbeCategoryEnum::kCachedVpdData};
  callback_done = false;
  ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      categories_to_test,
      base::BindOnce(
          [](bool* callback_done, mojom::TelemetryInfoPtr info) {
            EXPECT_EQ(info, MakeTelemetryInfo());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

}  // namespace
}  // namespace cros_healthd
}  // namespace chromeos
