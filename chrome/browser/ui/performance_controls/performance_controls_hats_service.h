// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_

#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

class PerformanceControlsHatsService
    : public KeyedService,
      public performance_manager::user_tuning::UserPerformanceTuningManager::
          Observer,
      public performance_manager::user_tuning::BatterySaverModeManager::
          Observer {
 public:
  explicit PerformanceControlsHatsService(Profile* profile);
  ~PerformanceControlsHatsService() override;

  // Called when the user opens an NTP. This allows the service to check if one
  // of the performance controls surveys should be shown.
  void OpenedNewTabPage();

  // BatterySaverModeManager::Observer:
  // Called in response to a change in the battery saver mode pref to check
  // whether a HaTS survey should be shown.
  void OnBatterySaverModeChanged(bool is_active) override;

  // performance_manager::user_tuning::UserPerformanceTuningManager::Observer:
  // Called in response to a change in the memory saver mode pref to check
  // whether a HaTS survey should be shown.
  void OnMemorySaverModeChanged() override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
