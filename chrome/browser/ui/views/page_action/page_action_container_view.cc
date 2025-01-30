// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/page_action/page_action_constants.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout.h"

namespace page_actions {

PageActionContainerView::PageActionContainerView(
    const std::vector<actions::ActionItem*>& action_items,
    IconLabelBubbleView::Delegate* icon_view_delegate) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);

  // Add 8 dip after each child.
  layout->SetDefault(views::kMarginsKey,
                     gfx::Insets().set_right(kPageActionBetweenIconSpacing));

  for (auto* action_item : action_items) {
    auto* child = AddChildView(
        std::make_unique<PageActionView>(action_item, icon_view_delegate));

    page_action_views_[action_item->GetActionId().value()] = child;

    child->SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kPreferredSnapToMinimum,
                           views::MaximumFlexSizeRule::kPreferred));
  }
}

PageActionContainerView::~PageActionContainerView() = default;

void PageActionContainerView::SetController(PageActionController* controller) {
  for (auto& [action_id, page_action_view] : page_action_views_) {
    page_action_view->OnNewActiveController(controller);
  }
}

PageActionView* PageActionContainerView::GetPageActionView(
    actions::ActionId action_id) {
  auto id_to_view = page_action_views_.find(action_id);
  return id_to_view != page_action_views_.end() ? id_to_view->second : nullptr;
}

BEGIN_METADATA(PageActionContainerView)
END_METADATA

}  // namespace page_actions
