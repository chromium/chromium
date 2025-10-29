// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_TOP_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_TOP_CONTAINER_H_

#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace views {
class ActionViewController;
class LabelButton;
}  // namespace views

// Top container of the vertical tab strip, manages the collapse and tab search
// buttons, accounting for space that might be needed for caption buttons.
class VerticalTabStripTopContainer : public views::View,
                                     public views::LayoutDelegate {
  METADATA_HEADER(VerticalTabStripTopContainer, views::View)

 public:
  VerticalTabStripTopContainer(
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item);
  ~VerticalTabStripTopContainer() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // Creates a TopContainerButton based on an ActionId.
  views::LabelButton* AddChildButtonFor(actions::ActionId action_id);

  views::LabelButton* GetTabSearchButton() { return tab_search_button_; }
  views::LabelButton* GetCollapseButton() { return collapse_button_; }

  bool IsPositionInWindowCaption(const gfx::Point& point);

 private:
  raw_ptr<tabs::VerticalTabStripStateController> state_controller_ = nullptr;
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<views::LabelButton> tab_search_button_ = nullptr;
  raw_ptr<views::LabelButton> collapse_button_ = nullptr;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_TOP_CONTAINER_H_
