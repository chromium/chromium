// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"

namespace new_tab_footer {

NewTabFooterController::NewTabFooterController(tabs::TabInterface* tab)
    : tab_(tab) {
  // TODO(crbug.com/4438803): Support SideBySide.
  if (!features::IsNtpFooterEnabledWithoutSideBySide()) {
    return;
  }

  auto* footer_web_view =
      tab_->GetBrowserWindowInterface()->NewTabFooterWebView();
  CHECK(footer_web_view);
  // TODO(crbug.com/409056427): Show/hide the footer based on what tab is being
  // used.
  footer_web_view->ShowUI();
}

}  // namespace new_tab_footer
