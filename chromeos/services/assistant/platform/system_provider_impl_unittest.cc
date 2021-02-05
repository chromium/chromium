// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/system_provider_impl.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "chromeos/services/assistant/platform/power_manager_provider_impl.h"
#include "chromeos/services/assistant/public/cpp/migration/fake_platform_delegate.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

class FakeBatteryMonitor : device::mojom::BatteryMonitor {
 public:
  FakeBatteryMonitor() {}

  mojo::PendingRemote<device::mojom::BatteryMonitor> CreateRemoteAndBind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void QueryNextStatus(QueryNextStatusCallback callback) override {
    if (has_status) {
      std::move(callback).Run(std::move(battery_status_));
      has_status = false;
    } else {
      callback_ = std::move(callback);
    }
  }

  void SetStatus(device::mojom::BatteryStatusPtr battery_status) {
    battery_status_ = std::move(battery_status);
    has_status = true;

    if (callback_) {
      std::move(callback_).Run(std::move(battery_status_));
      has_status = false;
    }
  }

 private:
  bool has_status = false;

  device::mojom::BatteryStatusPtr battery_status_;
  QueryNextStatusCallback callback_;

  mojo::Receiver<device::mojom::BatteryMonitor> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryMonitor);
};

class AssistantSystemProviderImplTest : public testing::Test {
 public:
  AssistantSystemProviderImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    battery_monitor_.SetStatus(device::mojom::BatteryStatus::New(
        false /* charging */, 0 /* charging_time */, 0 /* discharging_time */,
        0 /* level */));

    system_provider_impl_ = std::make_unique<SystemProviderImpl>(
        std::make_unique<PowerManagerProviderImpl>(
            task_environment_.GetMainThreadTaskRunner(), &platform_delegate),
        battery_monitor_.CreateRemoteAndBind());
    FlushForTesting();
  }

  SystemProviderImpl* system_provider() { return system_provider_impl_.get(); }

  FakeBatteryMonitor* battery_monitor() { return &battery_monitor_; }

  void FlushForTesting() { system_provider_impl_->FlushForTesting(); }

 private:
  base::test::TaskEnvironment task_environment_;
  FakeBatteryMonitor battery_monitor_;
  FakePlatformDelegate platform_delegate;
  std::unique_ptr<SystemProviderImpl> system_provider_impl_;

  DISALLOW_COPY_AND_ASSIGN(AssistantSystemProviderImplTest);
};

TEST_F(AssistantSystemProviderImplTest, GetBatteryStateReturnsLastState) {
  SystemProviderImpl::BatteryState state;
  // Initial level is 0
  system_provider()->GetBatteryState(&state);
  FlushForTesting();

  EXPECT_EQ(state.charge_percentage, 0);
  battery_monitor()->SetStatus(device::mojom::BatteryStatus::New(
      false /* charging */, 0 /* charging_time */, 0 /* discharging_time */,
      1 /* level */));

  FlushForTesting();
  // New level after status change
  system_provider()->GetBatteryState(&state);
  EXPECT_EQ(state.charge_percentage, 100);
}

}  // namespace assistant
}  // namespace chromeos
