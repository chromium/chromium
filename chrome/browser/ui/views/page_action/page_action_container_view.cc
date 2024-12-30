// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include <memory>

#include "chrome/browser/ui/views/page_action/page_action_constants.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/actions/action_view_controller.h"

namespace page_actions {

PageActionContainerView::PageActionContainerView(
    const std::vector<actions::ActionItem*>& action_items,
    IconLabelBubbleView::Delegate* icon_view_delegate)
    : action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetBetweenChildSpacing(kPageActionBetweenIconSpacing);

  // Right align to clip the leftmost items first when not enough space.
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);

  // Ensure that the same spacing that applies to the children is applied
  // between the PageActionIconContainerView and this container.
  SetInsideBorderInsets(gfx::Insets().set_right(kPageActionBetweenIconSpacing));

  for (actions::ActionItem* action_item : action_items) {
    PageActionView* view = AddChildView(
        std::make_unique<PageActionView>(action_item, icon_view_delegate));
    page_action_views_[action_item->GetActionId().value()] = view;
    action_view_controller_->CreateActionViewRelationship(
        view, action_item->GetAsWeakPtr());
  }
}

PageActionContainerView::~PageActionContainerView() = default;

void PageActionContainerView::SetController(PageActionController* controller) {
  for (auto& id_to_view : page_action_views_) {
    id_to_view.second->OnNewActiveController(controller);
  }
}

PageActionView* PageActionContainerView::GetPageActionView(
    actions::ActionId page_action_id) {
  auto id_to_view = page_action_views_.find(page_action_id);
  return id_to_view != page_action_views_.end() ? id_to_view->second : nullptr;
}

BEGIN_METADATA(PageActionContainerView)
END_METADATA

}  // namespace page_actions
