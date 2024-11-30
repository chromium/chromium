// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class ActionViewController;
}  // namespace views

namespace page_actions {

class PageActionController;
class PageActionView;

// PageActionContainerView is the parent view of all PageActionViews.
// TODO(crbug.com/376285664): Revisit the Layout View used, and make sure
// BoxLayoutView behaves well with AnimatingLayoutManager or switch to a
// different layout (e.g. FlexLayoutView).
class PageActionContainerView : public views::BoxLayoutView {
  METADATA_HEADER(PageActionContainerView, views::BoxLayoutView)
 public:
  PageActionContainerView(const std::vector<actions::ActionItem*>& action_items,
                          IconLabelBubbleView::Delegate* icon_view_delegate);
  PageActionContainerView(const PageActionContainerView&) = delete;
  PageActionContainerView& operator=(const PageActionContainerView&) = delete;
  ~PageActionContainerView() override;

  // Sets the active PageActionControllerfor each PageActionView.
  void SetController(PageActionController* controller);

 private:
  std::vector<raw_ptr<PageActionView>> page_action_views_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
