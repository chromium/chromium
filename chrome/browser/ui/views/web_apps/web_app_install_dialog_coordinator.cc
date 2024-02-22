// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_coordinator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

namespace web_app {

WebAppInstallDialogCoordinator::~WebAppInstallDialogCoordinator() = default;

bool WebAppInstallDialogCoordinator::IsShowing() {
  return install_dialog_tracker_.view() != nullptr;
}

PWAConfirmationBubbleView* WebAppInstallDialogCoordinator::GetBubbleView() {
  return IsShowing() ? views::AsViewClass<PWAConfirmationBubbleView>(
                           install_dialog_tracker_.view())
                     : nullptr;
}

void WebAppInstallDialogCoordinator::StartTracking(views::View* bubble_view) {
  CHECK(!IsShowing()) << "Cannot track a new install dialog if it an existing "
                         "one is already open";
  install_dialog_tracker_.SetView(bubble_view);
}

void WebAppInstallDialogCoordinator::StopTracking() {
  CHECK(IsShowing()) << "Cannot stop tracking install dialog when it was not "
                        "being tracked previously";
  install_dialog_tracker_.SetView(nullptr);
}

WebAppInstallDialogCoordinator::WebAppInstallDialogCoordinator(Browser* browser)
    : BrowserUserData<WebAppInstallDialogCoordinator>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(WebAppInstallDialogCoordinator);

}  // namespace web_app
