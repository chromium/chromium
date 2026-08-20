// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class BrowserWindowInterface;
class ExpandOnHoverLock;
class TabStripFlatEdgeButton;

namespace actions {
class ActionItem;
}  // namespace actions

namespace tabs {
class VerticalTabStripStateController;
enum class VerticalTabStripCollapseState;
}  // namespace tabs

// Bottom container of the vertical tab strip which includes the new tab button.
class VerticalTabStripBottomContainer : public views::FlexLayoutView {
  METADATA_HEADER(VerticalTabStripBottomContainer, views::FlexLayoutView)
 public:
  VerticalTabStripBottomContainer(
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item,
      BrowserWindowInterface* browser,
      base::RepeatingClosure record_new_tab_button_pressed);
  ~VerticalTabStripBottomContainer() override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  void OnCollapseStateChanged(tabs::VerticalTabStripCollapseState state);

 private:
  void OnNewTabButtonContextMenuWillShow();
  void OnNewTabButtonContextMenuClosed();

  void UpdateButtonStyles(bool collapsed);

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> new_tab_button_ = nullptr;
  base::CallbackListSubscription collapsed_state_change_subscription_;
  base::CallbackListSubscription new_tab_button_pressed_subscription_;

  std::unique_ptr<ExpandOnHoverLock> expand_on_hover_lock_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_BOTTOM_CONTAINER_H_
