// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_style.h"

#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// Thickness in DIPs of the separator painted on the left and right edges of
// the tab.
constexpr int kSeparatorThickness = 1;

// Returns the height of the separator between tabs.
int GetSeparatorHeight() {
  return ui::TouchUiController::Get()->touch_ui() ? 24 : 20;
}

class GM2TabStyle : public TabStyle {};
class ChromeRefresh2023TabStyle : public GM2TabStyle {};

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
  constexpr int kTabPinnedContentWidth = 24;
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
      views::Emphasis::kHigh);
}

// static
int TabStyle::GetContentsHorizontalInsetSize() {
  return GetCornerRadius() * 2;
}

std::unique_ptr<const TabStyle> TabStyle::Create() {
  // If refresh is turned on use ChromeRefresh23 styling.
  if (features::IsChromeRefresh2023()) {
    return std::make_unique<ChromeRefresh2023TabStyle>();
  }
  return std::make_unique<GM2TabStyle>();
}
