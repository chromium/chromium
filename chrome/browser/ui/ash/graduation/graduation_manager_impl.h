// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GRADUATION_GRADUATION_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GRADUATION_GRADUATION_MANAGER_IMPL_H_

#include <memory>

#include "ash/public/cpp/graduation/graduation_manager.h"
#include "ash/system/graduation/graduation_nudge_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

namespace base {
class Clock;
class TickClock;
class WallClockTimer;
}  // namespace base

namespace ash::graduation {

// Manages the state of the Transfer app depending on the status of the
// Graduation enablement policy. The GraduationManagerImpl is a singleton that
// should be created once per user session.
class GraduationManagerImpl : public ash::graduation::GraduationManager,
                              public session_manager::SessionManagerObserver {
 public:
  GraduationManagerImpl();
  GraduationManagerImpl(const GraduationManagerImpl&) = delete;
  GraduationManagerImpl& operator=(const GraduationManagerImpl&) = delete;
  ~GraduationManagerImpl() override;

  // ash::graduation::GraduationManager:
  std::string GetLanguageCode() const override;

  signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* context) override;
  content::StoragePartition* GetStoragePartition(
      content::BrowserContext* context,
      const content::StoragePartitionConfig& storage_partition_config) override;

  void AddObserver(GraduationManagerObserver* observer) override;
  void RemoveObserver(GraduationManagerObserver* observer) override;

  void SetClocksForTesting(const base::Clock* clock,
                           const base::TickClock* tick_clock) override;
  void ResumeTimerForTesting() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary) override;

 private:
  void OnAppsSynchronized();
  void OnWebAppProviderReady();
  void UpdateAppPinnedState();
  void OnPrefChanged();
  void OnMidnightTimer();
  void MaybeScheduleAppStatusUpdate();
  void UpdateAppReadiness();
  void NotifyAppUpdate();

  PrefChangeRegistrar pref_change_registrar_;

  // Profile object will be nullptr until the user session begins.
  raw_ptr<Profile> profile_ = nullptr;

  // The GraduationNudgeController is created during `OnUserSessionStarted`.
  std::unique_ptr<GraduationNudgeController> nudge_controller_;

  // The midnight timer is created during `OnUserSessionStarted`.
  std::unique_ptr<base::WallClockTimer> midnight_timer_;

  // Clocks that can be mocked in tests to set and fast-forward the time.
  // They are set to the default clock and tick clock instances when not mocked.
  raw_ptr<const base::Clock> clock_;
  raw_ptr<const base::TickClock> tick_clock_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  base::ObserverList<GraduationManagerObserver> observers_;

  base::WeakPtrFactory<GraduationManagerImpl> weak_ptr_factory_{this};
};
}  // namespace ash::graduation

#endif  // CHROME_BROWSER_UI_ASH_GRADUATION_GRADUATION_MANAGER_IMPL_H_
