// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OFFICIAL_BUILD)
#error Probe service unit tests should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/probe_service.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ProbeServiceTest : public testing::Test {
 public:
  void SetUp() override { CrosHealthdClient::InitializeFake(); }

  void TearDown() override {
    CrosHealthdClient::Shutdown();

    // Wait for ServiceConnection to observe the destruction of the client.
    cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

  health::mojom::ProbeServiceProxy* probe_service() const {
    return remote_probe_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<health::mojom::ProbeService> remote_probe_service_;
  ProbeService probe_service_{
      remote_probe_service_.BindNewPipeAndPassReceiver()};
};

// Tests that ProbeTelemetryInfo requests telemetry info in cros_healthd and
// forwards response via callback.
TEST_F(ProbeServiceTest, ProbeTelemetryInfoSuccess) {
  constexpr int64_t kCycleCount = 512;

  {
    auto battery_info = cros_healthd::mojom::BatteryInfo::New();
    battery_info->cycle_count = kCycleCount;

    auto info = cros_healthd::mojom::TelemetryInfo::New();
    info->battery_result = cros_healthd::mojom::BatteryResult::NewBatteryInfo(
        std::move(battery_info));

    cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);
  }

  base::RunLoop run_loop;
  probe_service()->ProbeTelemetryInfo(
      {health::mojom::ProbeCategoryEnum::kBattery},
      base::BindLambdaForTesting([&](health::mojom::TelemetryInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        ASSERT_TRUE(ptr->battery_result);
        ASSERT_TRUE(ptr->battery_result->is_battery_info());
        ASSERT_TRUE(ptr->battery_result->get_battery_info());
        ASSERT_TRUE(ptr->battery_result->get_battery_info()->cycle_count);
        EXPECT_EQ(ptr->battery_result->get_battery_info()->cycle_count->value,
                  kCycleCount);

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace chromeos
