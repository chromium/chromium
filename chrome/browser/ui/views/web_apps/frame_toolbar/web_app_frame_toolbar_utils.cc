// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/layout/layout_provider.h"

// An ink drop with round corners in shown when the user hovers over the button.
// Insets are kept small to avoid increasing web app frame toolbar height.
void SetInsetsForWebAppToolbarButton(ToolbarButton* toolbar_button,
                                     bool is_browser_focus_mode) {
  if (!is_browser_focus_mode)
    toolbar_button->SetLayoutInsets(gfx::Insets(2));
}

int WebAppFrameRightMargin() {
#if defined(OS_MAC)
  return kWebAppMenuMargin;
#else
  return HorizontalPaddingBetweenPageActionsAndAppMenuButtons();
#endif
}

int HorizontalPaddingBetweenPageActionsAndAppMenuButtons() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}
