// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout.h"

namespace page_actions {

PageActionContainerView::PageActionContainerView(
    const std::vector<actions::ActionItem*>& action_items,
    const PageActionViewParams& params) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);

  // Add `params.between_icon_spacing` dip after each child, except for the last
  // item, unless we need to bridge this container with icons to the right.
  layout
      ->SetDefault(views::kMarginsKey,
                   gfx::Insets().set_right(params.between_icon_spacing))
      .SetIgnoreDefaultMainAxisMargins(!params.should_bridge_containers);

  for (actions::ActionItem* action_item : action_items) {
    PageActionView* view =
        AddChildView(std::make_unique<PageActionView>(action_item, params));
    page_action_views_[action_item->GetActionId().value()] = view;

    view->SetProperty(views::kFlexBehaviorKey,
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
