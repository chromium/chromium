// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/test/test_device_status_listener.h"

#include <memory>

#include "base/bind.h"
#include "components/download/internal/background_service/scheduler/battery_status_listener_impl.h"
#include "components/download/network/network_status_listener_impl.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace download {
namespace test {

class FakeBatteryStatusListener : public BatteryStatusListenerImpl {
 public:
  FakeBatteryStatusListener() : BatteryStatusListenerImpl(base::TimeDelta()) {}
  ~FakeBatteryStatusListener() override = default;

  // BatteryStatusListener implementation.
  int GetBatteryPercentageInternal() override { return 100; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBatteryStatusListener);
};

TestDeviceStatusListener::TestDeviceStatusListener()
    : DeviceStatusListener(
          base::TimeDelta(), /* startup_delay */
          base::TimeDelta(), /* online_delay */
          std::make_unique<FakeBatteryStatusListener>(),
          std::make_unique<NetworkStatusListenerImpl>(
              network::TestNetworkConnectionTracker::GetInstance())) {}

TestDeviceStatusListener::~TestDeviceStatusListener() {
  // Mark |listening_| to false to bypass the remove observer calls in the base
  // class.
  Stop();
}

void TestDeviceStatusListener::NotifyObserver(
    const DeviceStatus& device_status) {
  status_ = device_status;
  DCHECK(observer_);
  observer_->OnDeviceStatusChanged(status_);
}

void TestDeviceStatusListener::SetDeviceStatus(const DeviceStatus& status) {
  status_ = status;
}

void TestDeviceStatusListener::Start(const base::TimeDelta& start_delay) {
  if (listening_ || !observer_)
    return;

  listening_ = true;

  // Simulates the delay after start up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestDeviceStatusListener::StartAfterDelay,
                                weak_ptr_factory_.GetWeakPtr()));
}

void TestDeviceStatusListener::StartAfterDelay() {
  is_valid_state_ = true;
  NotifyObserver(status_);
}

void TestDeviceStatusListener::Stop() {
  status_ = DeviceStatus();
  observer_ = nullptr;
  listening_ = false;
}

}  // namespace test
}  // namespace download
