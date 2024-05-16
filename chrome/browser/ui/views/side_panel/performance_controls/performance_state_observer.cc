// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/performance_controls/performance_state_observer.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"

PerformanceStateObserver::PerformanceStateObserver(Browser* browser)
    : browser_(browser) {
  battery_saver_mode_observation_.Observe(
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance());
}

PerformanceStateObserver::~PerformanceStateObserver() = default;

void PerformanceStateObserver::OnBatterySaverActiveChanged(bool is_active) {
  if (performance_manager::user_tuning::IsBatterySaverModeManagedByOS()) {
    return;
  }

  BrowserActions* const browser_actions = browser_->browser_actions();
  actions::ActionItem* const action_item =
      actions::ActionManager::Get().FindAction(
          kActionSidePanelShowPerformance, browser_actions->root_action_item());
  CHECK(action_item);
  if (is_active) {
    action_item->SetImage(
        ui::ImageModel::FromVectorIcon(kBatterySaverRefreshIcon));
  } else {
    action_item->SetImage(ui::ImageModel::FromVectorIcon(kPerformanceIcon));
  }
}
