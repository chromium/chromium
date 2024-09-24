// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/memory_saver_opt_in_iph_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

MemorySaverOptInIPHController::MemorySaverOptInIPHController(
    BrowserWindowInterface* interface)
    : browser_window_interface_(interface) {
  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  memory_saver_observer_.Observe(manager);
}

MemorySaverOptInIPHController::~MemorySaverOptInIPHController() = default;

void MemorySaverOptInIPHController::OnMemoryThresholdReached() {
  MaybeTriggerPromo();
}

void MemorySaverOptInIPHController::OnTabCountThresholdReached() {
  MaybeTriggerPromo();
}

void MemorySaverOptInIPHController::OnJankThresholdReached() {
  MaybeTriggerPromo();
}

void MemorySaverOptInIPHController::MaybeTriggerPromo() {
  auto* const manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  if (manager->IsMemorySaverModeDefault() &&
      !manager->IsMemorySaverModeActive()) {
    browser_window_interface_->GetUserEducationInterface()
        ->MaybeShowStartupFeaturePromo(
            feature_engagement::kIPHMemorySaverModeFeature);
  }
}
