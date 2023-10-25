// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/layout/layout_provider.h"

void ConfigureWebAppToolbarButton(
    ToolbarButton* toolbar_button,
    ToolbarButtonProvider* toolbar_button_provider) {
  // An ink drop with round corners in shown when the user hovers over the
  // button. Eliminate the insets to avoid increasing web app frame toolbar
  // height. The size of the button is set below.
  toolbar_button->SetLayoutInsets(gfx::Insets());

  toolbar_button->SetMinSize(toolbar_button_provider->GetToolbarButtonSize());
  toolbar_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  toolbar_button->SetAppearDisabledInInactiveWidget(true);
}

int WebAppFrameRightMargin() {
#if BUILDFLAG(IS_MAC)
  return kWebAppMenuMargin;
#else
  return HorizontalPaddingBetweenPageActionsAndAppMenuButtons();
#endif
}

int HorizontalPaddingBetweenPageActionsAndAppMenuButtons() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}
