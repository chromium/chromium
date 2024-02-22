// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_coordinator.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace web_app {

WebAppInstallDialogCoordinator::~WebAppInstallDialogCoordinator() {
  dialog_delegate_ = nullptr;
}

bool WebAppInstallDialogCoordinator::IsShowing() {
  return dialog_delegate_ != nullptr;
}

views::BubbleDialogDelegate* WebAppInstallDialogCoordinator::GetBubbleView() {
  return IsShowing() ? dialog_delegate_ : nullptr;
}

void WebAppInstallDialogCoordinator::StartTracking(
    views::BubbleDialogDelegate* bubble_view) {
  CHECK(!IsShowing()) << "Cannot track a new install dialog if an existing "
                         "one is already open";
  dialog_delegate_ = bubble_view;
}

void WebAppInstallDialogCoordinator::StopTracking() {
  CHECK(IsShowing()) << "Cannot stop tracking install dialog when it was not "
                        "being tracked previously";
  dialog_delegate_ = nullptr;
}

base::WeakPtr<WebAppInstallDialogCoordinator>
WebAppInstallDialogCoordinator::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WebAppInstallDialogCoordinator::WebAppInstallDialogCoordinator(Browser* browser)
    : BrowserUserData<WebAppInstallDialogCoordinator>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(WebAppInstallDialogCoordinator);

}  // namespace web_app
