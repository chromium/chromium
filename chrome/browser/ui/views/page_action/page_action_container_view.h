// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_

#include <list>
#include <map>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/actions/action_id.h"
#include "ui/views/layout/box_layout_view.h"

namespace page_actions {

class PageActionController;
class PageActionView;
class PageActionPropertiesProviderInterface;
struct PageActionViewParams;

// PageActionContainerView is the parent view of all PageActionViews.
class PageActionContainerView : public views::View {
  METADATA_HEADER(PageActionContainerView, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPageActionContainerViewElementId);

  PageActionContainerView(
      const std::vector<actions::ActionItem*>& action_items,
      const PageActionPropertiesProviderInterface& properties_provider,
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
  // Invoked when the chip state changes. When the view's suggestion chip is
  // shown, it is placed in the front before all other page action view.
  // Otherwise, the page action is placed in its initial insertion position.
  void OnPageActionSuggestionChipStateChanged(PageActionView* view);

  // Ensure the chip (if any) is at index 0 and all other actions are in
  // the correct relative order (after the chip).
  void NormalizePageActionViewOrder();

  std::map<actions::ActionId, raw_ptr<PageActionView>> page_action_views_;
  std::map<actions::ActionId, size_t> page_action_view_initial_indices_;

  // Callbacks used to handle page action view chip state changes. Used to
  // ensure that the container reorders the page actions accordingly.
  std::vector<base::CallbackListSubscription> chip_state_changed_callbacks_;

  // Subscriptions to page action view visibility changes, used to ensure that
  // the chips/non-chips page action view are correctly ordered.
  std::vector<base::CallbackListSubscription>
      page_action_view_visible_changed_callbacks_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
