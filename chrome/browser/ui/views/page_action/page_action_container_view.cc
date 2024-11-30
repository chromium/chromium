// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include <memory>

#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"

namespace page_actions {

PageActionContainerView::PageActionContainerView(
    const std::vector<actions::ActionItem*>& action_items,
    IconLabelBubbleView::Delegate* icon_view_delegate)
    : action_view_controller_(std::make_unique<views::ActionViewController>()) {
  for (actions::ActionItem* action_item : action_items) {
    PageActionView* view = AddChildView(
        std::make_unique<PageActionView>(action_item, icon_view_delegate));
    page_action_views_.emplace_back(view);
    action_view_controller_->CreateActionViewRelationship(
        view, action_item->GetAsWeakPtr());
  }
}

PageActionContainerView::~PageActionContainerView() = default;

void PageActionContainerView::SetController(PageActionController* controller) {
  for (PageActionView* view : page_action_views_) {
    view->OnNewActiveController(controller);
  }
}

BEGIN_METADATA(PageActionContainerView)
END_METADATA

}  // namespace page_actions
