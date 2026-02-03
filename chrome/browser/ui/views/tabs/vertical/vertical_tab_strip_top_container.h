// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_TOP_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_TOP_CONTAINER_H_

#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabStripFlatEdgeButton;
class BrowserWindowInterface;

namespace gfx {
class Point;
}  // namespace gfx

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace tab_groups {
class STGEverythingMenu;
}

namespace views {
class ActionViewController;
class LabelButton;
class MenuButtonController;
}  // namespace views

// Top container of the vertical tab strip, manages the collapse and tab search
// buttons, accounting for space that might be needed for caption buttons.
class VerticalTabStripTopContainer : public views::View,
                                     public views::LayoutDelegate {
  METADATA_HEADER(VerticalTabStripTopContainer, views::View)

 public:
  VerticalTabStripTopContainer(
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item,
      BrowserWindowInterface* browser);
  ~VerticalTabStripTopContainer() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // Creates a TopContainerButton (Collapse Button).
  views::LabelButton* AddTopContainerChildButtonFor(
      actions::ActionId action_id);
  // Creates FlatEdgeButton (Tab Groups & Tab Search).
  TabStripFlatEdgeButton* AddFlatEdgeChildButtonFor(
      actions::ActionId action_id);

  TabStripFlatEdgeButton* GetTabSearchButton() { return tab_search_button_; }
  views::LabelButton* GetCollapseButton() { return collapse_button_; }

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // These methods provide the toolbar height and exclusion width, before the
  // layout of this view, for use in calculating positioning of child views. If
  // an exclusion width is provided, nothing can be rendered within the
  // rectangle defined by `(caption_button_width, toolbar_height)` that is
  // aligned to the leading, top corner.
  void SetToolbarHeightForLayout(int toolbar_height);
  void SetCaptionButtonWidthForLayout(int caption_button_width);

 private:
  void ShowEverythingMenu();

  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* controller);

  // This method updates the flat edges of the tab search and tab group buttons
  // according to the collapsed state.
  void UpdateButtonStyles(tabs::VerticalTabStripStateController* controller);

  raw_ptr<tabs::VerticalTabStripStateController> state_controller_ = nullptr;
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> tab_search_button_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> tab_group_button_ = nullptr;
  raw_ptr<views::LabelButton> collapse_button_ = nullptr;

  raw_ptr<views::MenuButtonController> everything_menu_controller_ = nullptr;

  base::CallbackListSubscription collapsed_state_changed_subscription_;
  std::unique_ptr<tab_groups::STGEverythingMenu> everything_menu_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;

  // This represents the toolbar (element containing toolbar buttons, omnibox,
  // app menu, etc) height.
  int toolbar_height_ = 0;
  // When provided, a rectangle formed by the caption button width is anchored
  // to the top leading edge of this container and cannot have any elements
  // rendered inside of it becaused that area is reserved for outside UI
  // elements.
  int caption_button_width_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_TOP_CONTAINER_H_
