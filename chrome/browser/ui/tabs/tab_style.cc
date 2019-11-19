// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_style.h"

#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// Thickness in DIPs of the separator painted on the left and right edges of
// the tab.
constexpr int kSeparatorThickness = 1;

// Returns the height of the separator between tabs.
int GetSeparatorHeight() {
  return ui::MaterialDesignController::touch_ui() ? 24 : 20;
}

}  // namespace

TabStyle::~TabStyle() = default;

// static
int TabStyle::GetStandardWidth() {
  // The standard tab width is 240 DIP including both separators.
  constexpr int kTabWidth = 240;
  // The overlap includes one separator, so subtract it here.
  return kTabWidth + GetTabOverlap() - kSeparatorThickness;
}

// static
int TabStyle::GetPinnedWidth() {
  constexpr int kTabPinnedContentWidth = 23;
  return kTabPinnedContentWidth + GetContentsHorizontalInsetSize() * 2;
}

// static
int TabStyle::GetTabOverlap() {
  return GetCornerRadius() * 2 + kSeparatorThickness;
}

// static
int TabStyle::GetDragHandleExtension(int height) {
  return (height - GetSeparatorHeight()) / 2 - 1;
}

// static
gfx::Insets TabStyle::GetTabInternalPadding() {
  return gfx::Insets(0, GetCornerRadius());
}

// static
gfx::Size TabStyle::GetSeparatorSize() {
  return gfx::Size(kSeparatorThickness, GetSeparatorHeight());
}

// static
gfx::Size TabStyle::GetPreviewImageSize() {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetStandardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

// static
int TabStyle::GetCornerRadius() {
  return views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_HIGH);
}

// static
int TabStyle::GetContentsHorizontalInsetSize() {
  return GetCornerRadius() * 2;
}
