// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_OPT_IN_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_OPT_IN_IPH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

class BrowserWindowInterface;

class MemorySaverOptInIPHController
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          Observer {
 public:
  explicit MemorySaverOptInIPHController(BrowserWindowInterface* interface);
  ~MemorySaverOptInIPHController() override;

  MemorySaverOptInIPHController(const MemorySaverOptInIPHController&) = delete;
  MemorySaverOptInIPHController& operator=(
      const MemorySaverOptInIPHController&) = delete;

  // UserPerformanceTuningManager::Observer:
  void OnMemoryThresholdReached() override;
  void OnTabCountThresholdReached() override;
  void OnJankThresholdReached() override;

 private:
  void MaybeTriggerPromo();

  base::ScopedObservation<
      performance_manager::user_tuning::UserPerformanceTuningManager,
      performance_manager::user_tuning::UserPerformanceTuningManager::Observer>
      memory_saver_observer_{this};

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_OPT_IN_IPH_CONTROLLER_H_
