// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_iph_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

HighEfficiencyIPHController::HighEfficiencyIPHController(Browser* browser)
    : browser_(browser) {
  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  high_efficiency_observer_.Observe(manager);
}

HighEfficiencyIPHController::~HighEfficiencyIPHController() = default;

void HighEfficiencyIPHController::OnMemoryThresholdReached() {
  MaybeTriggerPromo();
}

void HighEfficiencyIPHController::OnTabCountThresholdReached() {
  MaybeTriggerPromo();
}

void HighEfficiencyIPHController::OnJankThresholdReached() {
  MaybeTriggerPromo();
}

void HighEfficiencyIPHController::MaybeTriggerPromo() {
  BrowserWindow* browser_window = browser_->window();
  PrefService* prefs = g_browser_process->local_state();
  if (browser_window != nullptr &&
      prefs
          ->FindPreference(performance_manager::user_tuning::prefs::
                               kHighEfficiencyModeEnabled)
          ->IsDefaultValue()) {
    browser_window->MaybeShowStartupFeaturePromo(
        feature_engagement::kIPHHighEfficiencyModeFeature);
  }
}
