// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageActionContainerView,
                                      kPageActionContainerViewElementId);

PageActionContainerView::PageActionContainerView(
    const std::vector<actions::ActionItem*>& action_items,
    const PageActionPropertiesProviderInterface& properties_provider,
    const PageActionViewParams& params) {
  SetProperty(views::kElementIdentifierKey, kPageActionContainerViewElementId);

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);

  // Add `params.between_icon_spacing` dip after each child, except for the last
  // item, unless we need to bridge this container with icons to the right.
  layout
      ->SetDefault(views::kMarginsKey,
                   gfx::Insets().set_right(params.between_icon_spacing))
      .SetIgnoreDefaultMainAxisMargins(!params.should_bridge_containers);

  size_t initial_index = 0;
  for (actions::ActionItem* action_item : action_items) {
    const auto action_item_id = action_item->GetActionId().value();
    const auto& properties = properties_provider.GetProperties(action_item_id);

    // When the page action migration is not enabled, the view should not be
    // created to avoid conflicting with the old framework version identifier.
    if (!IsPageActionMigrated(properties.type)) {
      continue;
    }

    PageActionView* view = AddChildView(std::make_unique<PageActionView>(
        action_item, params, properties.element_identifier));

    page_action_views_[action_item_id] = view;
    chip_state_changed_callbacks_.push_back(
        view->AddChipVisibilityChangedCallback(base::BindRepeating(
            &PageActionContainerView::OnPageActionSuggestionChipStateChanged,
            base::Unretained(this))));

    page_action_view_visible_changed_callbacks_.push_back(
        view->AddVisibleChangedCallback(base::BindRepeating(
            &PageActionContainerView::NormalizePageActionViewOrder,
            base::Unretained(this))));

    // Record the original index for the page action view so that even if it
    // become a suggestion chip (move to index 0) we can bring it back later at
    // the exact same initial index.
    page_action_view_initial_indices_[action_item_id] = initial_index++;

    view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            params.hide_icon_on_space_constraint
                ? views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero
                : views::MinimumFlexSizeRule::kPreferredSnapToMinimum,
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

void PageActionContainerView::OnPageActionSuggestionChipStateChanged(
    PageActionView* view) {
  NormalizePageActionViewOrder();
}

void PageActionContainerView::NormalizePageActionViewOrder() {
  std::vector<std::pair<size_t /*initial_index*/, PageActionView*>> chips;
  std::vector<std::pair<size_t /*initial_index*/, PageActionView*>> non_chips;

  chips.reserve(page_action_views_.size());
  non_chips.reserve(page_action_views_.size());

  for (const auto& [action_id, view] : page_action_views_) {
    const auto it = page_action_view_initial_indices_.find(action_id);
    CHECK(it != page_action_view_initial_indices_.end());

    const size_t initial_index = it->second;
    (view->IsChipVisible() ? chips : non_chips)
        .emplace_back(initial_index, view);
  }

  // Sort both groups by initial insertion index to keep stable, predictable
  // order.
  auto by_initial_index = [](const auto& a, const auto& b) {
    return a.first < b.first;
  };
  std::sort(chips.begin(), chips.end(), by_initial_index);
  std::sort(non_chips.begin(), non_chips.end(), by_initial_index);

  // Place all chips first, in initial-order.
  size_t next_index = 0;
  for (const auto& entry : chips) {
    ReorderChildView(entry.second, next_index++);
  }

  // Place the rest, offset by the number of chips.
  for (const auto& entry : non_chips) {
    ReorderChildView(entry.second, next_index++);
  }
}

BEGIN_METADATA(PageActionContainerView)
END_METADATA

}  // namespace page_actions
