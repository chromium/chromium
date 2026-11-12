// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_STRIP_OVERFLOW_INDICATOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_STRIP_OVERFLOW_INDICATOR_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserView;

namespace views {
enum class OverflowIndicatorAlignment;
}

// Draws an overflow indicator with an outer opaque region and inner shadow
// gradient over the edge of the horizontal tab strip when content overflows
// the viewport.
class HorizontalTabStripOverflowIndicatorView : public views::View {
  METADATA_HEADER(HorizontalTabStripOverflowIndicatorView, views::View)

 public:
  static constexpr int kShadowThickness = 6;
  static int GetMaxOpaqueThickness();
  static int GetMaxTotalThickness();

  HorizontalTabStripOverflowIndicatorView(
      views::OverflowIndicatorAlignment side,
      BrowserView* browser_view);
  HorizontalTabStripOverflowIndicatorView(
      const HorizontalTabStripOverflowIndicatorView&) = delete;
  HorizontalTabStripOverflowIndicatorView& operator=(
      const HorizontalTabStripOverflowIndicatorView&) = delete;
  ~HorizontalTabStripOverflowIndicatorView() override;

  int opaque_thickness() const { return opaque_thickness_; }
  void SetOpaqueThickness(int opaque_thickness);

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const views::OverflowIndicatorAlignment side_;
  const raw_ptr<BrowserView> browser_view_;
  int opaque_thickness_ = 0;
  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_STRIP_OVERFLOW_INDICATOR_VIEW_H_
