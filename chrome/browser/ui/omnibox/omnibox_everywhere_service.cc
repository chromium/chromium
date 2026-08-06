// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

OmniboxEverywhereService::OmniboxEverywhereService(Profile* profile)
    : profile_(profile) {}

OmniboxEverywhereService::~OmniboxEverywhereService() {
  Shutdown();
}

omnibox_everywhere::OmniboxEverywhereController*
OmniboxEverywhereService::controller() const {
  return g_browser_process && g_browser_process->GetFeatures()
             ? g_browser_process->GetFeatures()->omnibox_everywhere_controller()
             : nullptr;
}

void OmniboxEverywhereService::Shutdown() {
  if (controller()) {
    controller()->ShutdownForProfile(profile_);
  }
}

void OmniboxEverywhereService::HidePopup() {
  if (controller()) {
    controller()->Close();
  }
}

bool OmniboxEverywhereService::IsPopupVisible() const {
  return controller() && controller()->IsVisible();
}

void OmniboxEverywhereService::ShowProfilePicker() {
  if (controller()) {
    controller()->ShowProfilePicker();
  }
}

void OmniboxEverywhereService::SetIsNavigating(bool is_navigating) {
  if (controller() && controller()->ui_manager()) {
    controller()->ui_manager()->SetIsNavigating(is_navigating);
  }
}

void OmniboxEverywhereService::OnDrivePickerOpened() {
  if (controller() && controller()->ui_manager()) {
    controller()->ui_manager()->OnDrivePickerOpened();
  }
}

void OmniboxEverywhereService::OnDrivePickerClosed() {
  if (controller() && controller()->ui_manager()) {
    controller()->ui_manager()->OnDrivePickerClosed();
  }
}

void OmniboxEverywhereService::OpenUrl(const GURL& url,
                                       WindowOpenDisposition disposition,
                                       ui::PageTransition transition) {
  SetIsNavigating(true);
  HidePopup();

  auto* browser_collection = ProfileBrowserCollection::GetForProfile(profile_);
  CHECK(browser_collection);
  BrowserWindowInterface* bwi  =
      browser_collection->GetLastActiveBrowser();
  bool is_new_window = false;
  if (!bwi) {
    bwi = chrome::OpenEmptyWindow(profile_);
    is_new_window = true;
  }

  if (bwi) {
    NavigateParams params(bwi, url, transition);
    params.disposition =
        is_new_window ? WindowOpenDisposition::CURRENT_TAB
                      : ((disposition == WindowOpenDisposition::CURRENT_TAB)
                             ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                             : disposition);
    params.window_action = NavigateParams::WindowAction::kShowWindow;
    Navigate(&params);
  }
}
