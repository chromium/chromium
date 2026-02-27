// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_web_ui_manager.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(InitialWebUIManager);

InitialWebUIManager::InitialWebUIManager(BrowserWindowInterface* browser)
    : is_initial_web_ui_pending_(features::IsWebUIToolbarEnabled()),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}

InitialWebUIManager::~InitialWebUIManager() = default;

InitialWebUIManager* InitialWebUIManager::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

bool InitialWebUIManager::RequestDeferShow(base::OnceClosure unsafe_callback) {
  if (!base::FeatureList::IsEnabled(features::kWebUIReloadButton) &&
      !features::kWebUIReloadButtonDeferBrowserViewShow.Get()) {
    return false;
  }
  if (is_initial_web_ui_pending_) {
    is_show_pending_ = true;
    if (unsafe_callback) {
      web_ui_ready_callbacks_.AddUnsafe(std::move(unsafe_callback));
    }
    return true;
  }
  return false;
}

bool InitialWebUIManager::IsShowPending() const {
  return is_show_pending_;
}

void InitialWebUIManager::OnWebUIToolbarLoaded() {
  is_initial_web_ui_pending_ = false;
  is_show_pending_ = false;
  web_ui_ready_callbacks_.Notify();
}
