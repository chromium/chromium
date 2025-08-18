// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/layout/delegating_layout_manager.h"

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace views {
class ResizeArea;
class Separator;
class View;
}  // namespace views

// Container for the vertical tabstrip and the other views sharing space with
// it, excluding the caption buttons.
class VerticalTabStripRegionView final : public views::AccessiblePaneView,
                                         public views::ResizeAreaDelegate,
                                         public views::LayoutDelegate {
  METADATA_HEADER(VerticalTabStripRegionView, views::AccessiblePaneView)

 public:
  static constexpr int kResizeAreaWidth = 6;

  explicit VerticalTabStripRegionView(
      tabs::VerticalTabStripStateController* state_controller);
  VerticalTabStripRegionView(const VerticalTabStripRegionView&) = delete;
  VerticalTabStripRegionView& operator=(const VerticalTabStripRegionView&) =
      delete;
  ~VerticalTabStripRegionView() override;

  views::Separator* tabs_separator_for_testing() { return tabs_separator_; }
  views::ResizeArea* resize_area_for_testing() { return resize_area_; }
  views::View* pinned_tabs_container_for_testing() {
    return pinned_tabs_container_;
  }
  views::View* unpinned_tabs_container_for_testing() {
    return unpinned_tabs_container_;
  }

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

 private:
  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* state_controller);

  raw_ptr<views::View> top_button_container_ = nullptr;
  raw_ptr<views::Separator> top_button_separator_ = nullptr;
  raw_ptr<views::View> pinned_tabs_container_ = nullptr;
  raw_ptr<views::Separator> tabs_separator_ = nullptr;
  raw_ptr<views::View> unpinned_tabs_container_ = nullptr;
  raw_ptr<views::View> segmented_button_ = nullptr;
  raw_ptr<views::View> gemini_button_ = nullptr;
  raw_ptr<views::ResizeArea> resize_area_ = nullptr;

  base::CallbackListSubscription collapsed_state_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_
