// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_

#include <list>
#include <map>

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/actions/action_id.h"
#include "ui/views/layout/box_layout_view.h"

namespace page_actions {

class PageActionController;
class PageActionView;
struct PageActionViewParams;

// PageActionContainerView is the parent view of all PageActionViews.
class PageActionContainerView : public views::View {
  METADATA_HEADER(PageActionContainerView, views::View)
 public:
  PageActionContainerView(const std::vector<actions::ActionItem*>& action_items,
                          const PageActionViewParams& params);
  PageActionContainerView(const PageActionContainerView&) = delete;
  PageActionContainerView& operator=(const PageActionContainerView&) = delete;
  ~PageActionContainerView() override;

  // Sets the active PageActionController for each PageActionView.
  void SetController(PageActionController* controller);

  // Gets the PageActionView associated with the given action id. Returns
  // nullptr if not found.
  PageActionView* GetPageActionView(actions::ActionId page_action_id);

 private:
  // Invoked when the chip state changes. When `suggestion_chip_visible` is
  // true, the page action associated with `action_id` is placed in the front
  // before all other page actions. Otherwise, the page action is placed in it's
  // initial insertion position.
  void OnPageActionSuggestionChipStateChanged(actions::ActionId action_id,
                                              bool suggestion_chip_visible);

  std::map<actions::ActionId, raw_ptr<PageActionView>> page_action_views_;
  std::map<actions::ActionId, int> page_action_view_initial_indices_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
