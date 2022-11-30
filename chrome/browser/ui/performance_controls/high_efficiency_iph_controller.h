// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_IPH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser.h"

class HighEfficiencyIPHController : public performance_manager::user_tuning::
                                        UserPerformanceTuningManager::Observer {
 public:
  explicit HighEfficiencyIPHController(Browser* browser);
  ~HighEfficiencyIPHController() override;

  HighEfficiencyIPHController(const HighEfficiencyIPHController&) = delete;
  HighEfficiencyIPHController& operator=(const HighEfficiencyIPHController&) =
      delete;

  // UserPerformanceTuningManager::Observer:
  void OnMemoryThresholdReached() override;
  void OnTabCountThresholdReached() override;
  void OnJankThresholdReached() override;

 private:
  void MaybeTriggerPromo();

  base::ScopedObservation<
      performance_manager::user_tuning::UserPerformanceTuningManager,
      performance_manager::user_tuning::UserPerformanceTuningManager::Observer>
      high_efficiency_observer_{this};

  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_IPH_CONTROLLER_H_
