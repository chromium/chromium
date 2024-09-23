// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cec_service/fake_cec_service_client.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace ash {

FakeCecServiceClient::FakeCecServiceClient() = default;
FakeCecServiceClient::~FakeCecServiceClient() = default;

void FakeCecServiceClient::SendStandBy() {
  stand_by_call_count_++;
  SetDisplayPowerState(PowerState::kStandBy);
}

void FakeCecServiceClient::SendWakeUp() {
  wake_up_call_count_++;
  SetDisplayPowerState(PowerState::kOn);
}

void FakeCecServiceClient::QueryDisplayCecPowerState(
    CecServiceClient::PowerStateCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), tv_power_states_));
}

void FakeCecServiceClient::Init(dbus::Bus* bus) {}

void FakeCecServiceClient::SetDisplayPowerState(PowerState new_state) {
  for (size_t i = 0; i < tv_power_states_.size(); i++) {
    tv_power_states_[i] = new_state;
  }
}

}  // namespace ash
