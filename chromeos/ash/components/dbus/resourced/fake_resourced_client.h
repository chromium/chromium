// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"

namespace ash {

class COMPONENT_EXPORT(RESOURCED) FakeResourcedClient : public ResourcedClient {
 public:
  FakeResourcedClient();
  ~FakeResourcedClient() override;

  FakeResourcedClient(const FakeResourcedClient&) = delete;
  FakeResourcedClient& operator=(const FakeResourcedClient&) = delete;

  // ResourcedClient:
  void SetGameModeWithTimeout(
      GameMode game_mode,
      uint32_t refresh_seconds,
      chromeos::DBusMethodCallback<GameMode> callback) override;

  void SetMemoryMarginsBps(uint32_t critical,
                           uint32_t moderate,
                           SetMemoryMarginsBpsCallback callback) override;

  void set_total_system_memory(uint64_t mem_kb) {
    total_system_memory_kb_ = mem_kb;
  }

  void set_set_game_mode_response(absl::optional<GameMode> response) {
    set_game_mode_response_ = response;
  }

  void set_set_game_mode_with_timeout_response(
      absl::optional<GameMode> response) {
    previous_game_mode_state_ = response;
  }

  int get_enter_game_mode_count() const { return enter_game_mode_count_; }

  int get_exit_game_mode_count() const { return exit_game_mode_count_; }

  uint32_t get_critical_margin_bps() const { return critical_margin_bps_; }
  uint32_t get_moderate_margin_bps() const { return moderate_margin_bps_; }

  void AddObserver(Observer* observer) override;

  void RemoveObserver(Observer* observer) override;

  void AddArcVmObserver(ArcVmObserver* observer) override;
  void RemoveArcVmObserver(ArcVmObserver* observer) override;

  void FakeArcVmMemoryPressure(PressureLevelArcVm level,
                               uint64_t reclaim_target_kb);

  void AddArcContainerObserver(ArcContainerObserver* observer) override;
  void RemoveArcContainerObserver(ArcContainerObserver* observer) override;

 private:
  absl::optional<GameMode> set_game_mode_response_;
  absl::optional<GameMode> previous_game_mode_state_ = GameMode::OFF;

  int enter_game_mode_count_ = 0;
  int exit_game_mode_count_ = 0;

  uint64_t total_system_memory_kb_ = 1000 * 1000; /* 1 gb */
  uint32_t critical_margin_bps_ = 520;
  uint32_t moderate_margin_bps_ = 4000;

  base::ObserverList<Observer> observers_;
  base::ObserverList<ArcVmObserver> arcvm_observers_;
  base::ObserverList<ArcContainerObserver> arc_container_observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
