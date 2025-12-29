// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_

#include "ui/views/layout/flex_layout_view.h"

class BottomContainerButton;
class BrowserWindowInterface;

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace tab_groups {
class STGEverythingMenu;
}  // namespace tab_groups

namespace views {
class ActionViewController;
class MenuButtonController;
}  // namespace views

// Bottom container of the vertical tab strip, manages the new tab and tab group
// buttons.
class VerticalTabStripBottomContainer : public views::FlexLayoutView {
  METADATA_HEADER(VerticalTabStripBottomContainer, views::View)
 public:
  VerticalTabStripBottomContainer(
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item,
      BrowserWindowInterface* browser);
  ~VerticalTabStripBottomContainer() override;

  BottomContainerButton* AddChildButtonFor(actions::ActionId action_id);

  void ShowEverythingMenu();

  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* state_controller);

 private:
  void UpdateButtonStyles(
      tabs::VerticalTabStripStateController* state_controller);

  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<BottomContainerButton> new_tab_button_ = nullptr;
  raw_ptr<BottomContainerButton> tab_group_button_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<views::MenuButtonController> everything_menu_controller_ = nullptr;
  base::CallbackListSubscription collapsed_state_changed_subscription_;

  views::FlexSpecification uncollapsed_flex_specification_;
  views::FlexSpecification collapsed_flex_specification_;

  std::unique_ptr<tab_groups::STGEverythingMenu> everything_menu_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
