// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_

#include <list>
#include <map>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/actions/action_id.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

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

  // Returns the height of the capsule container, derived dynamically from the
  // location bar height and element padding.
  static int GetCapsuleHeight();

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

  // Returns true if the first visible view inside this container is a chip.
  bool IsFirstVisibleViewChip() const;

  // Returns true if the elevated capsule background is currently active.
  bool IsCapsuleActive() const { return is_capsule_active_; }

  // views::View:
  void ChildVisibilityChanged(views::View* child) override;

 private:
  // Invoked when the chip or anchored message state changes. We show the
  // anchored message (if any), then suggestion chips then all other page action
  // icons. Within its category, the page action is placed in its initial
  // insertion position.
  void OnPageActionStateChanged(PageActionView* view);

  // Ensure the chip (if any) is at index 0 and all other actions are in
  // the correct relative order (after the chip).
  void NormalizePageActionViewOrder();

  void UpdateBackgroundAndMargins();

  raw_ptr<views::FlexLayout> layout_ = nullptr;
  bool is_capsule_active_ = false;
  int between_icon_spacing_ = 0;

  std::map<actions::ActionId, raw_ptr<PageActionView>> page_action_views_;
  std::map<actions::ActionId, size_t> page_action_view_initial_indices_;

  // Callbacks used to handle page action view state changes (chip, anchored
  // message, and visibility) to reorder children and update capsule styling.
  std::vector<base::CallbackListSubscription>
      page_action_state_changed_callbacks_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTAINER_VIEW_H_
