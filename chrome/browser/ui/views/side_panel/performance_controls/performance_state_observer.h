// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_STATE_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_STATE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/ui/browser.h"

// Observer that updates the performance toolbar button based on performance
// resource usage
class PerformanceStateObserver : public performance_manager::user_tuning::
                                     BatterySaverModeManager::Observer {
 public:
  explicit PerformanceStateObserver(Browser* browser);

  PerformanceStateObserver& operator=(const PerformanceStateObserver&) = delete;
  ~PerformanceStateObserver() override;

  // BatterySaverModeManager::Observer:
  void OnBatterySaverActiveChanged(bool is_active) override;

 private:
  raw_ptr<Browser> browser_ = nullptr;

  base::ScopedObservation<
      performance_manager::user_tuning::BatterySaverModeManager,
      performance_manager::user_tuning::BatterySaverModeManager::Observer>
      battery_saver_mode_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_STATE_OBSERVER_H_
