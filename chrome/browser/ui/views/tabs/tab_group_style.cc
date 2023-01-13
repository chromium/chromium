// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_style.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

TabGroupStyle::TabGroupStyle(const TabGroupViews& tab_group_views)
    : tab_group_views_(tab_group_views) {}

TabGroupStyle::~TabGroupStyle() = default;

bool TabGroupStyle::TabGroupUnderlineShouldBeHidden() const {
  return false;
}

bool TabGroupStyle::TabGroupUnderlineShouldBeHidden(
    const views::View* const leading_view,
    const views::View* const trailing_view) const {
  return false;
}

ChromeRefresh2023TabGroupStyle::ChromeRefresh2023TabGroupStyle(
    const TabGroupViews& tab_group_views)
    : TabGroupStyle(tab_group_views) {}

ChromeRefresh2023TabGroupStyle::~ChromeRefresh2023TabGroupStyle() = default;

bool ChromeRefresh2023TabGroupStyle::TabGroupUnderlineShouldBeHidden() const {
  const auto [leading_group_view, trailing_group_view] =
      tab_group_views_->GetLeadingTrailingGroupViews();

  return TabGroupUnderlineShouldBeHidden(leading_group_view,
                                         trailing_group_view);
}

bool ChromeRefresh2023TabGroupStyle::TabGroupUnderlineShouldBeHidden(
    const views::View* const leading_view,
    const views::View* const trailing_view) const {
  const TabGroupHeader* const leading_view_group_header =
      views::AsViewClass<TabGroupHeader>(leading_view);
  const TabGroupHeader* const trailing_view_group_header =
      views::AsViewClass<TabGroupHeader>(trailing_view);

  if (leading_view_group_header && trailing_view_group_header &&
      leading_view_group_header == trailing_view_group_header) {
    return true;
  }

  return false;
}
