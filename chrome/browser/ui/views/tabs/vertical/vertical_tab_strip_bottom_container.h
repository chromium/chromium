// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_

#include "ui/views/layout/flex_layout_view.h"

class TabStripFlatEdgeButton;

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace views {
class ActionViewController;
}  // namespace views

// Bottom container of the vertical tab strip which includes the new tab button.
class VerticalTabStripBottomContainer : public views::FlexLayoutView {
  METADATA_HEADER(VerticalTabStripBottomContainer, views::View)
 public:
  VerticalTabStripBottomContainer(
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item,
      base::RepeatingClosure record_new_tab_button_pressed);
  ~VerticalTabStripBottomContainer() override;

  TabStripFlatEdgeButton* AddChildButtonFor(actions::ActionId action_id);

  bool IsPositionInWindowCaption(const gfx::Point& point);

  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* state_controller);

 private:
  void UpdateButtonStyles(
      tabs::VerticalTabStripStateController* state_controller);

  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> new_tab_button_ = nullptr;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
  base::CallbackListSubscription new_tab_button_pressed_subscription_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
