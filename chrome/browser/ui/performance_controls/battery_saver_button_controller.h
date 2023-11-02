// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

class BatterySaverButtonControllerDelegate;

// This class controls the visibility of the battery saver toolbar button.
// It registers for battery saver mode change events with the user performance
// tuning manager and utilizes the delegate interface to update the visibility.
class BatterySaverButtonController
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          Observer {
 public:
  BatterySaverButtonController();
  ~BatterySaverButtonController() override;

  BatterySaverButtonController(const BatterySaverButtonController&) = delete;
  BatterySaverButtonController& operator=(const BatterySaverButtonController&) =
      delete;

  // Init starts listening for performance manager events
  void Init(BatterySaverButtonControllerDelegate* delegate);

  // UserPerformanceTuningManager::Observer:
  void OnBatterySaverModeChanged(bool is_active) override;

 private:
  void UpdateVisibilityState(bool is_active);

 private:
  raw_ptr<BatterySaverButtonControllerDelegate> delegate_ = nullptr;
  base::ScopedObservation<
      performance_manager::user_tuning::UserPerformanceTuningManager,
      performance_manager::user_tuning::UserPerformanceTuningManager::Observer>
      battery_saver_observer_{this};
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_CONTROLLER_H_
