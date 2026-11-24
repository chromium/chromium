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

// Draws an inner shadow gradient over the edge of a horizontal tab strip
// scroll view when content overflows, inset past any clipped viewport overlap
// (`overlap_`).
class HorizontalTabStripOverflowIndicatorView : public views::View {
  METADATA_HEADER(HorizontalTabStripOverflowIndicatorView, views::View)

 public:
  static constexpr int kShadowThickness = 6;
  static int GetMaxOverlap();
  static int GetMaxTotalThickness();

  explicit HorizontalTabStripOverflowIndicatorView(
      views::OverflowIndicatorAlignment side);
  HorizontalTabStripOverflowIndicatorView(
      const HorizontalTabStripOverflowIndicatorView&) = delete;
  HorizontalTabStripOverflowIndicatorView& operator=(
      const HorizontalTabStripOverflowIndicatorView&) = delete;
  ~HorizontalTabStripOverflowIndicatorView() override;

  int overlap() const { return overlap_; }
  void SetOverlap(int overlap);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const views::OverflowIndicatorAlignment side_;
  int overlap_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_STRIP_OVERFLOW_INDICATOR_VIEW_H_
