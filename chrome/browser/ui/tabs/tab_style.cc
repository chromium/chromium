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

class GM2TabStyle : public TabStyle {
 public:
  ~GM2TabStyle() override = default;
};
class ChromeRefresh2023TabStyle : public GM2TabStyle {
 public:
  ~ChromeRefresh2023TabStyle() override = default;
};

}  // namespace

TabStyle::~TabStyle() = default;

int TabStyle::GetStandardWidth() const {
  // The standard tab width is 240 DIP including both separators.
  constexpr int kTabWidth = 240;
  // The overlap includes one separator, so subtract it here.
  return kTabWidth + GetTabOverlap() - kSeparatorThickness;
}

int TabStyle::GetPinnedWidth() const {
  constexpr int kTabPinnedContentWidth = 24;
  return kTabPinnedContentWidth + GetContentsHorizontalInsetSize() * 2;
}

int TabStyle::GetTabOverlap() const {
  return GetCornerRadius() * 2 + kSeparatorThickness;
}

int TabStyle::GetDragHandleExtension(int height) const {
  return (height - GetSeparatorHeight()) / 2 - 1;
}

gfx::Size TabStyle::GetSeparatorSize() const {
  return gfx::Size(kSeparatorThickness, GetSeparatorHeight());
}

gfx::Size TabStyle::GetPreviewImageSize() const {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetStandardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

int TabStyle::GetCornerRadius() const {
  return views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}

int TabStyle::GetContentsHorizontalInsetSize() const {
  return GetCornerRadius() * 2;
}

float TabStyle::GetSelectedTabOpacity() const {
  return kDefaultSelectedTabOpacity;
}

// static
const TabStyle* TabStyle::Get() {
  static TabStyle* const tab_style =
      features::IsChromeRefresh2023()
          ? static_cast<TabStyle*>(new ChromeRefresh2023TabStyle())
          : static_cast<TabStyle*>(new GM2TabStyle());

  return tab_style;
}
