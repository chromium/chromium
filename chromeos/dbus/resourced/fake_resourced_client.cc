// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/resourced/fake_resourced_client.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeResourcedClient::FakeResourcedClient() = default;
FakeResourcedClient::~FakeResourcedClient() = default;

void FakeResourcedClient::SetGameModeWithTimeout(
    bool state,
    uint32_t refresh_seconds,
    DBusMethodCallback<bool> callback) {
  absl::optional<bool> response = previous_game_mode_state_;
  if (state) {
    enter_game_mode_count_++;
    previous_game_mode_state_ = true;
  } else {
    exit_game_mode_count_++;
    previous_game_mode_state_ = false;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeResourcedClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeResourcedClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeResourcedClient::AddArcVmObserver(ArcVmObserver* observer) {
  arcvm_observers_.AddObserver(observer);
}

void FakeResourcedClient::RemoveArcVmObserver(ArcVmObserver* observer) {
  arcvm_observers_.RemoveObserver(observer);
}

void FakeResourcedClient::FakeArcVmMemoryPressure(PressureLevelArcVm level,
                                                  uint64_t reclaim_target_kb) {
  for (auto& observer : arcvm_observers_) {
    observer.OnMemoryPressure(level, reclaim_target_kb);
  }
}

}  // namespace chromeos
