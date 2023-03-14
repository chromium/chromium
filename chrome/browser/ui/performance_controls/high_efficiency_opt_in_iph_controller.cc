// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_opt_in_iph_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

HighEfficiencyOptInIPHController::HighEfficiencyOptInIPHController(
    Browser* browser)
    : browser_(browser) {
  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  high_efficiency_observer_.Observe(manager);
}

HighEfficiencyOptInIPHController::~HighEfficiencyOptInIPHController() = default;

void HighEfficiencyOptInIPHController::OnMemoryThresholdReached() {
  MaybeTriggerPromo();
}

void HighEfficiencyOptInIPHController::OnTabCountThresholdReached() {
  MaybeTriggerPromo();
}

void HighEfficiencyOptInIPHController::OnJankThresholdReached() {
  MaybeTriggerPromo();
}

void HighEfficiencyOptInIPHController::MaybeTriggerPromo() {
  BrowserWindow* browser_window = browser_->window();
  if (browser_window != nullptr &&
      performance_manager::user_tuning::UserPerformanceTuningManager::
          GetInstance()
              ->IsHighEfficiencyModeDefault()) {
    browser_window->MaybeShowStartupFeaturePromo(
        feature_engagement::kIPHHighEfficiencyModeFeature);
  }
}
