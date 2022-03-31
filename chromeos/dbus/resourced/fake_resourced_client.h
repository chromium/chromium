// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
#define CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/dbus/resourced/resourced_client.h"

namespace chromeos {

class COMPONENT_EXPORT(RESOURCED) FakeResourcedClient : public ResourcedClient {
 public:
  FakeResourcedClient();
  ~FakeResourcedClient() override;

  FakeResourcedClient(const FakeResourcedClient&) = delete;
  FakeResourcedClient& operator=(const FakeResourcedClient&) = delete;

  // ResourcedClient:
  void SetGameModeWithTimeout(bool state,
                              uint32_t refresh_seconds,
                              DBusMethodCallback<bool> callback) override;

  void SetMemoryMarginsBps(uint32_t critical,
                           uint32_t moderate,
                           SetMemoryMarginsBpsCallback callback) override;

  void set_total_system_memory(uint64_t mem_kb) {
    total_system_memory_kb_ = mem_kb;
  }

  void set_set_game_mode_response(absl::optional<bool> response) {
    set_game_mode_response_ = response;
  }

  void set_set_game_mode_with_timeout_response(absl::optional<bool> response) {
    previous_game_mode_state_ = response;
  }

  int get_enter_game_mode_count() const { return enter_game_mode_count_; }

  int get_exit_game_mode_count() const { return exit_game_mode_count_; }

  void AddObserver(Observer* observer) override;

  void RemoveObserver(Observer* observer) override;

  void AddArcVmObserver(ArcVmObserver* observer) override;
  void RemoveArcVmObserver(ArcVmObserver* observer) override;

  void FakeArcVmMemoryPressure(PressureLevelArcVm level,
                               uint64_t reclaim_target_kb);

 private:
  absl::optional<bool> set_game_mode_response_;
  absl::optional<bool> previous_game_mode_state_ = false;

  int enter_game_mode_count_ = 0;
  int exit_game_mode_count_ = 0;

  uint64_t total_system_memory_kb_ = 1000 * 1000; /* 1 gb */
  uint32_t critical_margin_bps_ = 520;
  uint32_t moderate_margin_bps_ = 4000;

  base::ObserverList<Observer> observers_;
  base::ObserverList<ArcVmObserver> arcvm_observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
