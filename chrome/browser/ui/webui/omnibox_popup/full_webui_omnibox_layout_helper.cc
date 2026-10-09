// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/full_webui_omnibox_layout_helper.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography_provider.h"

// static
gfx::Insets FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets() {
  return FullWebUIOmniboxFrame::GetLocationBarAlignmentInsets();
}

// static
int FullWebUIOmniboxLayoutHelper::GetLocationBarHeight() {
  return GetLayoutConstant(LayoutConstant::kLocationBarHeight);
}

// static
int FullWebUIOmniboxLayoutHelper::GetLocationBarPageInfoIconVerticalPadding() {
  return GetLayoutConstant(
      LayoutConstant::kLocationBarPageInfoIconVerticalPadding);
}

// static
int FullWebUIOmniboxLayoutHelper::GetLocationBarIconSize() {
  return GetLayoutConstant(LayoutConstant::kLocationBarIconSize);
}

// static
int FullWebUIOmniboxLayoutHelper::GetFontSize() {
  if (views::LayoutProvider::Get()) {
    return views::TypographyProvider::Get()
        .GetFont(CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY)
        .GetFontSize();
  }
  return ui::TouchUiController::Get()->touch_ui() ? 15 : 14;
}

// static
void FullWebUIOmniboxLayoutHelper::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  const gfx::Insets insets = GetLocationBarAlignmentInsets();
  source->AddInteger("alignmentInsetTop", insets.top());
  source->AddInteger("alignmentInsetHorizontal", insets.left());
  source->AddInteger("alignmentInsetBottom", insets.bottom());
  source->AddInteger("locationBarHeight", GetLocationBarHeight());
  source->AddInteger("locationBarPageInfoIconVerticalPadding",
                     GetLocationBarPageInfoIconVerticalPadding());
  source->AddInteger("locationBarIconSize", GetLocationBarIconSize());
  source->AddInteger("locationBarFontSize", GetFontSize());
}
