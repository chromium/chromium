// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_web_ui_manager.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/base_window.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(InitialWebUIManager);

InitialWebUIManager::InitialWebUIManager(BrowserWindowInterface* browser)
    : window_(browser->GetWindow()),
      is_initial_web_ui_pending_(features::IsWebUIReloadButtonEnabled()),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}

InitialWebUIManager::~InitialWebUIManager() = default;

InitialWebUIManager* InitialWebUIManager::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

bool InitialWebUIManager::ShouldDeferShow() {
  if (is_initial_web_ui_pending_) {
    is_show_pending_ = true;
    return true;
  }
  return false;
}

void InitialWebUIManager::OnReloadButtonLoaded() {
  is_initial_web_ui_pending_ = false;
  MaybeShowBrowserWindow();
}

void InitialWebUIManager::MaybeShowBrowserWindow() {
  if (is_show_pending_ && !is_initial_web_ui_pending_) {
    is_show_pending_ = false;
    if (window_) {
      window_->Show();
    }
  }
}
