// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/resize_area_delegate.h"

class BrowserWindowInterface;
class RootTabCollectionNode;
class VerticalUnpinnedTabContainerView;
class VerticalPinnedTabContainerView;
class VerticalTabStripBottomContainer;
class VerticalTabStripTopContainer;

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace tabs_api {
class TabStripService;
}  // namespace tabs_api

namespace views {
class ResizeArea;
class Separator;
class View;
}  // namespace views

// Container for the vertical tabstrip and the other views sharing space with
// it, excluding the caption buttons.
class VerticalTabStripRegionView final : public views::AccessiblePaneView,
                                         public views::ResizeAreaDelegate {
  METADATA_HEADER(VerticalTabStripRegionView, views::AccessiblePaneView)

 public:
  static constexpr int kResizeAreaWidth = 6;

  explicit VerticalTabStripRegionView(
      tabs_api::TabStripService* service_register,
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item,
      BrowserWindowInterface* browser);
  VerticalTabStripRegionView(const VerticalTabStripRegionView&) = delete;
  VerticalTabStripRegionView& operator=(const VerticalTabStripRegionView&) =
      delete;
  ~VerticalTabStripRegionView() override;

  views::Separator* tabs_separator_for_testing() {
    return tab_strip_view_->tabs_separator_for_testing();
  }
  views::ResizeArea* resize_area_for_testing() { return resize_area_; }
  VerticalPinnedTabContainerView* pinned_tabs_container_for_testing() {
    return tab_strip_view_->GetPinnedTabsContainerForTesting();
  }
  VerticalUnpinnedTabContainerView* unpinned_tabs_container_for_testing() {
    return tab_strip_view_->GetUnpinnedTabsContainerForTesting();
  }

  VerticalTabStripTopContainer* GetTopContainer() {
    return top_button_container_;
  }

  VerticalTabStripBottomContainer* GetBottomContainer() {
    return bottom_button_container_;
  }

  // views::View:
  void Layout(PassKey) override;

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

 private:
  views::View* SetTabStripView(std::unique_ptr<views::View> view);

  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* state_controller);

  raw_ptr<VerticalTabStripTopContainer> top_button_container_ = nullptr;
  raw_ptr<views::Separator> top_button_separator_ = nullptr;
  raw_ptr<VerticalTabStripView> tab_strip_view_ = nullptr;
  raw_ptr<VerticalTabStripBottomContainer> bottom_button_container_ = nullptr;
  raw_ptr<views::View> gemini_button_ = nullptr;
  raw_ptr<views::ResizeArea> resize_area_ = nullptr;
  std::unique_ptr<RootTabCollectionNode> root_node_;

  raw_ptr<tabs::VerticalTabStripStateController> state_controller_;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_
