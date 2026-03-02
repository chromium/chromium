// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "ui/base/base_window.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(InitialWebUIWindowMetricsManager);

InitialWebUIWindowMetricsManager::InitialWebUIWindowMetricsManager(
    BrowserWindowInterface* browser)
    : waap_service_(WaapUIMetricsService::Get(browser->GetProfile())),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}

InitialWebUIWindowMetricsManager::~InitialWebUIWindowMetricsManager() = default;

InitialWebUIWindowMetricsManager* InitialWebUIWindowMetricsManager::From(
    BrowserWindowInterface* browser_window_interface) {
  return browser_window_interface
             ? Get(browser_window_interface->GetUnownedUserDataHost())
             : nullptr;
}

void InitialWebUIWindowMetricsManager::SetWindowCreationInfo(
    waap::NewWindowCreationSource source,
    base::TimeTicks creation_time) {
  creation_source_ = source;
  new_window_start_time_ = creation_time;
}

void InitialWebUIWindowMetricsManager::OnBrowserWindowFirstPresentation(
    base::TimeTicks timestamp) {
  // Ensures only one startup window is recorded per browser process.
  static bool is_startup_first_paint_recorded = false;
  if (!waap_service_) {
    return;
  }

  if (!browser_window_first_paint_time_.has_value()) {
    browser_window_first_paint_time_ = timestamp;
    RecordPaintDeltaIfAvailable();
  }

  if (!is_startup_first_paint_recorded && !skip_startup_metrics_for_testing_) {
    is_startup_first_paint_recorded = true;
    is_new_window_first_paint_recorded_ = true;
    waap_service_->OnBrowserWindowFirstPresentation(timestamp);
  } else if (!is_new_window_first_paint_recorded_ &&
             new_window_start_time_.has_value()) {
    is_new_window_first_paint_recorded_ = true;
    waap_service_->OnNewWindowBrowserWindowFirstPresentation(
        creation_source_, *new_window_start_time_, timestamp);
  }
}

void InitialWebUIWindowMetricsManager::OnBrowserWindowCreated() {
  if (waap_service_) {
    waap_service_->OnBrowserWindowCreated();
  }
}

void InitialWebUIWindowMetricsManager::OnReloadButtonCreated() {
  if (waap_service_) {
    waap_service_->OnReloadButtonCreated();
  }
}

void InitialWebUIWindowMetricsManager::OnReloadButtonFirstPaint(
    base::TimeTicks timestamp) {
  // Ensures only one startup reload button is recorded per browser process.
  static bool is_startup_first_paint_recorded = false;
  if (!waap_service_) {
    return;
  }

  if (!reload_button_first_paint_time_.has_value()) {
    reload_button_first_paint_time_ = timestamp;
    RecordPaintDeltaIfAvailable();
  }

  if (!is_startup_first_paint_recorded && !skip_startup_metrics_for_testing_) {
    is_startup_first_paint_recorded = true;
    is_new_window_reload_button_first_paint_recorded_ = true;
    waap_service_->OnFirstPaint(timestamp);
  } else if (!is_new_window_reload_button_first_paint_recorded_ &&
             new_window_start_time_.has_value()) {
    is_new_window_reload_button_first_paint_recorded_ = true;
    waap_service_->OnNewWindowReloadButtonFirstPaint(
        creation_source_, *new_window_start_time_, timestamp);
  }
}

void InitialWebUIWindowMetricsManager::OnReloadButtonFirstContentfulPaint(
    base::TimeTicks timestamp) {
  // Ensures only one startup reload button is recorded per browser process.
  static bool is_startup_first_contentful_paint_recorded = false;
  if (!waap_service_) {
    return;
  }

  if (!is_startup_first_contentful_paint_recorded &&
      !skip_startup_metrics_for_testing_) {
    is_startup_first_contentful_paint_recorded = true;
    is_new_window_reload_button_first_contentful_paint_recorded_ = true;
    waap_service_->OnFirstContentfulPaint(timestamp);
  } else if (!is_new_window_reload_button_first_contentful_paint_recorded_ &&
             new_window_start_time_.has_value()) {
    is_new_window_reload_button_first_contentful_paint_recorded_ = true;
    waap_service_->OnNewWindowReloadButtonFirstContentfulPaint(
        creation_source_, *new_window_start_time_, timestamp);
  }
}

void InitialWebUIWindowMetricsManager::SkipStartupForTesting() {
  skip_startup_metrics_for_testing_ = true;
}

void InitialWebUIWindowMetricsManager::RecordPaintDeltaIfAvailable() {
  if (!waap_service_ || !browser_window_first_paint_time_.has_value() ||
      !reload_button_first_paint_time_.has_value()) {
    return;
  }

  base::TimeDelta delta =
      *reload_button_first_paint_time_ - *browser_window_first_paint_time_;

  // We only care about positive deltas.
  if (delta.is_negative()) {
    return;
  }

  // Handle Startup metrics only once per process.
  static bool process_startup_delta_recorded = false;
  if (!process_startup_delta_recorded && !skip_startup_metrics_for_testing_) {
    process_startup_delta_recorded = true;
    startup_delta_recorded_ = true;
    waap_service_->OnStartupBrowserWindowToReloadButtonFirstPaintGap(
        *browser_window_first_paint_time_, *reload_button_first_paint_time_);
  } else if (!new_window_delta_recorded_ &&
             new_window_start_time_.has_value()) {
    // Handle New Window metrics once per window.
    new_window_delta_recorded_ = true;
    waap_service_->OnNewWindowBrowserWindowToReloadButtonFirstPaintGap(
        creation_source_, *browser_window_first_paint_time_,
        *reload_button_first_paint_time_);
  }
}
