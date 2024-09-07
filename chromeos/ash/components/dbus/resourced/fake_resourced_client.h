// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"

namespace ash {

class COMPONENT_EXPORT(RESOURCED) FakeResourcedClient : public ResourcedClient {
 public:
  FakeResourcedClient();
  ~FakeResourcedClient() override;

  FakeResourcedClient(const FakeResourcedClient&) = delete;
  FakeResourcedClient& operator=(const FakeResourcedClient&) = delete;

  struct SetThreadStateRequest {
    base::ProcessId process_id;
    base::PlatformThreadId thread_id;
    resource_manager::ThreadState state;
  };

  // ResourcedClient:
  void SetGameModeWithTimeout(
      GameMode game_mode,
      uint32_t refresh_seconds,
      chromeos::DBusMethodCallback<GameMode> callback) override;

  void SetMemoryMargins(MemoryMargins margins) override;

  void ReportBrowserProcesses(Component component,
                              const std::vector<Process>& processes) override;

  void SetProcessState(base::ProcessId,
                       resource_manager::ProcessState,
                       SetQoSStateCallback) override;
  void SetThreadState(base::ProcessId,
                      base::PlatformThreadId,
                      resource_manager::ThreadState,
                      SetQoSStateCallback) override;

  void set_total_system_memory(uint64_t mem_kb) {
    total_system_memory_kb_ = mem_kb;
  }

  void set_set_game_mode_response(std::optional<GameMode> response) {
    set_game_mode_response_ = response;
  }

  void set_set_game_mode_with_timeout_response(
      std::optional<GameMode> response) {
    previous_game_mode_state_ = response;
  }

  int get_enter_game_mode_count() const { return enter_game_mode_count_; }

  int get_exit_game_mode_count() const { return exit_game_mode_count_; }

  uint32_t get_moderate_margin_bps() const { return moderate_margin_bps_; }
  uint32_t get_critical_margin_bps() const { return critical_margin_bps_; }
  uint32_t get_critical_protected_margin_bps() const {
    return critical_protected_margin_bps_;
  }

  std::vector<int32_t> get_ash_background_pids() {
    return ash_background_pids_;
  }
  std::vector<int32_t> get_lacros_background_pids() {
    return lacros_background_pids_;
  }

  void AddObserver(Observer* observer) override;

  void RemoveObserver(Observer* observer) override;

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  void AddArcContainerObserver(ArcContainerObserver* observer) override;
  void RemoveArcContainerObserver(ArcContainerObserver* observer) override;

  // Unblock registered WaitForServiceToBeAvailable() calls.
  //
  // Return `true` if at least 1 WaitForServiceToBeAvailable() has been
  // called.
  bool TriggerServiceAvailable(bool available);
  // Return the list of process states that have been requested via
  // SetProcessState().
  const std::vector<std::pair<base::ProcessId, resource_manager::ProcessState>>&
  GetProcessStateHistory() const;
  // Return the list of thread states that have been requested via
  // SetThreadState().
  const std::vector<SetThreadStateRequest>& GetThreadStateHistory() const;
  // Set response for next SetProcessState() calls.
  void SetProcessStateResult(dbus::DBusResult);
  // Set response for next SetThreadState() calls.
  void SetThreadStateResult(dbus::DBusResult);
  // Delays the response of the next SetProcessStateResult calls.
  void DelaySetProcessStateResult(base::TimeDelta);
  // Delays the response of the next SetThreadStateResult calls.
  void DelaySetThreadStateResult(base::TimeDelta);

 private:
  std::optional<GameMode> set_game_mode_response_;
  std::optional<GameMode> previous_game_mode_state_ = GameMode::OFF;

  int enter_game_mode_count_ = 0;
  int exit_game_mode_count_ = 0;

  uint64_t total_system_memory_kb_ = 1000 * 1000; /* 1 gb */
  uint32_t moderate_margin_bps_ = 4000;
  uint32_t critical_margin_bps_ = 520;
  uint32_t critical_protected_margin_bps_ = 520;

  std::vector<int32_t> ash_background_pids_;
  std::vector<int32_t> lacros_background_pids_;
  std::vector<Process> ash_browser_processes_;
  std::vector<Process> lacros_browser_processes_;

  std::vector<dbus::ObjectProxy::WaitForServiceToBeAvailableCallback>
      pending_service_available_;
  std::vector<std::pair<base::ProcessId, resource_manager::ProcessState>>
      process_state_history_;
  std::vector<SetThreadStateRequest> thread_state_history_;

  dbus::DBusResult set_process_state_result_ = dbus::DBusResult::kSuccess;
  dbus::DBusResult set_thread_state_result_ = dbus::DBusResult::kSuccess;
  base::TimeDelta set_process_state_delay_;
  base::TimeDelta set_thread_state_delay_;

  base::ObserverList<Observer> observers_;
  base::ObserverList<ArcContainerObserver> arc_container_observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
