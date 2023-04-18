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
constexpr int kGM2SeparatorThickness = 1;
constexpr int kChromeRefreshSeparatorThickness = 2;

// Returns the height of the separator between tabs.
int GetSeparatorHeight() {
  return ui::TouchUiController::Get()->touch_ui() ? 24 : 20;
}

class GM2TabStyle : public TabStyle {
 public:
  ~GM2TabStyle() override = default;
  int GetStandardWidth() const override;
  int GetPinnedWidth() const override;
  int GetTabOverlap() const override;
  gfx::Size GetSeparatorSize() const override;
  int GetDragHandleExtension(int height) const override;
  gfx::Size GetPreviewImageSize() const override;
  int GetTopCornerRadius() const override;
  int GetBottomCornerRadius() const override;
  float GetSelectedTabOpacity() const override;
};
class ChromeRefresh2023TabStyle : public GM2TabStyle {
 public:
  ~ChromeRefresh2023TabStyle() override = default;
  int GetTopCornerRadius() const override;
  int GetBottomCornerRadius() const override;
  gfx::Size GetSeparatorSize() const override;
};

}  // namespace

TabStyle::~TabStyle() = default;

int GM2TabStyle::GetStandardWidth() const {
  // The standard tab width is 240 DIP including both separators.
  constexpr int kTabWidth = 240;
  // The overlap includes one separator, so subtract it here.
  return kTabWidth + GetTabOverlap() - GetSeparatorSize().width();
}

int GM2TabStyle::GetPinnedWidth() const {
  constexpr int kTabPinnedContentWidth = 24;
  return kTabPinnedContentWidth + GetContentsHorizontalInsetSize() * 2;
}

int GM2TabStyle::GetTabOverlap() const {
  return GetBottomCornerRadius() * 2 + GetSeparatorSize().width();
}

int GM2TabStyle::GetDragHandleExtension(int height) const {
  return (height - GetSeparatorSize().height()) / 2 - 1;
}

gfx::Size GM2TabStyle::GetSeparatorSize() const {
  return gfx::Size(kGM2SeparatorThickness, GetSeparatorHeight());
}

gfx::Size GM2TabStyle::GetPreviewImageSize() const {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetStandardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

int GM2TabStyle::GetTopCornerRadius() const {
  return views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}

int GM2TabStyle::GetBottomCornerRadius() const {
  return views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}

int TabStyle::GetContentsHorizontalInsetSize() const {
  return GetBottomCornerRadius() * 2;
}

float GM2TabStyle::GetSelectedTabOpacity() const {
  return kDefaultSelectedTabOpacity;
}

int ChromeRefresh2023TabStyle::GetTopCornerRadius() const {
  return 10;
}

int ChromeRefresh2023TabStyle::GetBottomCornerRadius() const {
  return 12;
}

gfx::Size ChromeRefresh2023TabStyle::GetSeparatorSize() const {
  return gfx::Size(kChromeRefreshSeparatorThickness, GetSeparatorHeight());
}

// static
const TabStyle* TabStyle::Get() {
  static TabStyle* const tab_style =
      features::IsChromeRefresh2023()
          ? static_cast<TabStyle*>(new ChromeRefresh2023TabStyle())
          : static_cast<TabStyle*>(new GM2TabStyle());

  return tab_style;
}
