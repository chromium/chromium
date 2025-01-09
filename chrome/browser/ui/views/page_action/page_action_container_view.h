// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_

#include <list>

#include "base/callback_list.h"
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

  // Gets the PageActionView associated with the given action id. Returns
  // nullptr if not found.
  PageActionView* GetPageActionView(actions::ActionId page_action_id);

 private:
  // Updates the container insets depending on it current state. Following
  // can happen:
  // 1. `page_action_views_` is empty or all views in `page_action_views_` are
  // not visible. In this case, the right inset will be 0.
  // 2. At least one of the views in `page_action_views_` is visible. In the
  // case, the right inset will be set to the appropriate value.
  //
  // TODO(crbug.com/384969003): After the page actions migration, this right
  // spacing will no longer be needed.
  void SetContainerInsideBorderInsets();

  std::map<actions::ActionId, raw_ptr<PageActionView>> page_action_views_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::list<base::CallbackListSubscription>
      page_action_views_visible_subscriptions_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
