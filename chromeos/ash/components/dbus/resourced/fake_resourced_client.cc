// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace ash {

FakeResourcedClient::FakeResourcedClient() = default;
FakeResourcedClient::~FakeResourcedClient() = default;

void FakeResourcedClient::SetGameModeWithTimeout(
    GameMode state,
    uint32_t refresh_seconds,
    chromeos::DBusMethodCallback<GameMode> callback) {
  std::optional<GameMode> response = previous_game_mode_state_;
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

void FakeResourcedClient::ReportBrowserProcesses(
    Component component,
    const std::vector<Process>& processes) {
  if (component == ResourcedClient::Component::kAsh) {
    ash_browser_processes_ = processes;
  } else if (component == ResourcedClient::Component::kLacros) {
    lacros_browser_processes_ = processes;
  } else {
    NOTREACHED();
  }
}

void FakeResourcedClient::SetProcessState(base::ProcessId process_id,
                                          resource_manager::ProcessState state,
                                          SetQoSStateCallback callback) {
  process_state_history_.push_back({process_id, state});
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), set_process_state_result_),
      set_process_state_delay_);
}

void FakeResourcedClient::SetThreadState(base::ProcessId process_id,
                                         base::PlatformThreadId thread_id,
                                         resource_manager::ThreadState state,
                                         SetQoSStateCallback callback) {
  thread_state_history_.push_back({process_id, thread_id, state});
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), set_thread_state_result_),
      set_thread_state_delay_);
}

void FakeResourcedClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  pending_service_available_.push_back(std::move(callback));
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

bool FakeResourcedClient::TriggerServiceAvailable(bool available) {
  if (pending_service_available_.empty()) {
    return false;
  }
  for (auto& callback : pending_service_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), available));
  }
  pending_service_available_.clear();
  return true;
}

const std::vector<std::pair<base::ProcessId, resource_manager::ProcessState>>&
FakeResourcedClient::GetProcessStateHistory() const {
  return process_state_history_;
}

const std::vector<FakeResourcedClient::SetThreadStateRequest>&
FakeResourcedClient::GetThreadStateHistory() const {
  return thread_state_history_;
}

void FakeResourcedClient::SetProcessStateResult(dbus::DBusResult result) {
  set_process_state_result_ = result;
}

void FakeResourcedClient::SetThreadStateResult(dbus::DBusResult result) {
  set_thread_state_result_ = result;
}

void FakeResourcedClient::DelaySetProcessStateResult(base::TimeDelta delay) {
  set_process_state_delay_ = delay;
}

void FakeResourcedClient::DelaySetThreadStateResult(base::TimeDelta delay) {
  set_thread_state_delay_ = delay;
}

void FakeResourcedClient::AddArcContainerObserver(
    ArcContainerObserver* observer) {
  arc_container_observers_.AddObserver(observer);
}

void FakeResourcedClient::RemoveArcContainerObserver(
    ArcContainerObserver* observer) {
  arc_container_observers_.RemoveObserver(observer);
}

}  // namespace ash
