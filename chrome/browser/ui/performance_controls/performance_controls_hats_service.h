// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PerformanceControlsHatsService
    : public KeyedService,
      public performance_manager::user_tuning::UserPerformanceTuningManager::
          Observer {
 public:
  explicit PerformanceControlsHatsService(Profile* profile);
  ~PerformanceControlsHatsService() override;

  // Called in response to a change in the battery saver mode pref to check
  // whether a HaTS survey should be shown.
  void OnBatterySaverModeChange();

  // Called when the user opens an NTP. This allows the service to check if one
  // of the performance controls surveys should be shown.
  void OpenedNewTabPage();

  // performance_manager::user_tuning::UserPerformanceTuningManager::Observer:
  // Called in response to a change in the high efficiency mode pref to check
  // whether a HaTS survey should be shown.
  void OnHighEfficiencyModeChanged() override;

 private:
  raw_ptr<Profile> profile_;
  PrefChangeRegistrar local_pref_registrar_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
