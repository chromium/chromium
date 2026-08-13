// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_STRIP_OVERFLOW_INDICATOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_STRIP_OVERFLOW_INDICATOR_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
enum class OverflowIndicatorAlignment;
}

// Draws an overflow indicator gradient over the edge of the horizontal tab
// strip when content overflows the viewport.
class HorizontalTabStripOverflowIndicatorView : public views::View {
  METADATA_HEADER(HorizontalTabStripOverflowIndicatorView, views::View)

 public:
  static constexpr int kTotalThickness = 6;

  explicit HorizontalTabStripOverflowIndicatorView(
      views::OverflowIndicatorAlignment side);
  HorizontalTabStripOverflowIndicatorView(
      const HorizontalTabStripOverflowIndicatorView&) = delete;
  HorizontalTabStripOverflowIndicatorView& operator=(
      const HorizontalTabStripOverflowIndicatorView&) = delete;
  ~HorizontalTabStripOverflowIndicatorView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const views::OverflowIndicatorAlignment side_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_STRIP_OVERFLOW_INDICATOR_VIEW_H_
