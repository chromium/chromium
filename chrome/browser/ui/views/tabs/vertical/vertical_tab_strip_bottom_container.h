// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_

#include "ui/views/context_menu_controller.h"
#include "ui/views/layout/flex_layout_view.h"

class BrowserWindowInterface;
class ExpandOnHoverLock;
class NewTabButtonMenuModel;
class TabStripFlatEdgeButton;

namespace tabs {
class VerticalTabStripStateController;
enum class VerticalTabStripCollapseState;
}  // namespace tabs

namespace views {
class ActionViewController;
class MenuRunner;
}  // namespace views

// Bottom container of the vertical tab strip which includes the new tab button.
class VerticalTabStripBottomContainer : public views::FlexLayoutView,
                                        public views::ContextMenuController {
  METADATA_HEADER(VerticalTabStripBottomContainer, views::View)
 public:
  VerticalTabStripBottomContainer(
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item,
      BrowserWindowInterface* browser,
      base::RepeatingClosure record_new_tab_button_pressed);
  ~VerticalTabStripBottomContainer() override;

  TabStripFlatEdgeButton* AddChildButtonFor(actions::ActionId action_id);

  bool IsPositionInWindowCaption(const gfx::Point& point);

  void OnCollapseStateChanged(tabs::VerticalTabStripCollapseState state);

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

 private:
  void OnNewTabButtonContextMenuClosed();

  void UpdateButtonStyles(bool collapsed);

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> new_tab_button_ = nullptr;
  base::CallbackListSubscription collapsed_state_change_subscription_;
  base::CallbackListSubscription new_tab_button_pressed_subscription_;

  std::unique_ptr<NewTabButtonMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<ExpandOnHoverLock> expand_on_hover_lock_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
