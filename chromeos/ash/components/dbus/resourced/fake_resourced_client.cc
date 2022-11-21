// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"

#include "base/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeResourcedClient::FakeResourcedClient() = default;
FakeResourcedClient::~FakeResourcedClient() = default;

void FakeResourcedClient::SetGameModeWithTimeout(
    GameMode state,
    uint32_t refresh_seconds,
    chromeos::DBusMethodCallback<GameMode> callback) {
  absl::optional<GameMode> response = previous_game_mode_state_;
  if (state == GameMode::OFF) {
    exit_game_mode_count_++;
  } else {
    enter_game_mode_count_++;
  }
  previous_game_mode_state_ = state;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeResourcedClient::SetMemoryMarginsBps(
    uint32_t critical_bps,
    uint32_t moderate_bps,
    SetMemoryMarginsBpsCallback callback) {
  critical_margin_bps_ = critical_bps;
  moderate_margin_bps_ = moderate_bps;

  uint32_t critical_kb = static_cast<uint32_t>(
      total_system_memory_kb_ * ((critical_margin_bps_ / 100.0) / 100.0));
  uint32_t moderate_kb = static_cast<uint32_t>(
      total_system_memory_kb_ * ((moderate_margin_bps_ / 100.0) / 100.0));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, critical_kb, moderate_kb));
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

}  // namespace ash
