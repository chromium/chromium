// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "chrome/common/chrome_features.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace page_actions {

namespace {

// Spacing inside the capsule container.
constexpr int kCapsuleInteriorPadding = 2;

constexpr int kIconSpacing = 2;

constexpr int kChipSpacing = 5;

}  // namespace

class PageActionContainerView::PendingPageAction
    : public PageActionModelObserver {
 public:
  PendingPageAction(PageActionContainerView& container,
                    actions::ActionItem& action_item,
                    PageActionProperties properties)
      : container_(container),
        action_item_(action_item),
        properties_(properties) {}
  PendingPageAction(const PendingPageAction&) = delete;
  PendingPageAction& operator=(const PendingPageAction&) = delete;
  ~PendingPageAction() override = default;

  void SetController(PageActionController* new_controller) {
    if (new_controller == controller_) {
      return;
    }
    observation_.Reset();
    action_item_subscription_ = {};
    controller_ = new_controller;
    if (!new_controller) {
      return;
    }
    PageActionContainerView& container = *container_;
    actions::ActionItem& action_item = *action_item_;
    const actions::ActionId id = action_item.GetActionId().value();
    // `CreateActionItemSubscription()` synchronously notifies existing
    // observers, which could materialize this action and destroy `this`.
    base::CallbackListSubscription subscription =
        new_controller->CreateActionItemSubscription(&action_item);
    if (!container.pending_actions_.contains(id)) {
      return;
    }
    action_item_subscription_ = std::move(subscription);
    new_controller->AddObserver(id, observation_);
    if (!container.pending_actions_.contains(id)) {
      return;
    }
    if (observation_.IsObserving() && observation_.GetSource()->GetVisible()) {
      container.MaterializePageActionView(id);
    }
  }

  actions::ActionItem* action_item() const { return &action_item_.get(); }
  const PageActionProperties& properties() const { return properties_; }
  PageActionController* controller() const { return controller_.get(); }

 private:
  // PageActionModelObserver:
  void OnPageActionModelChanged(
      const PageActionModelInterface& model) override {
    if (model.GetVisible()) {
      container_->MaterializePageActionView(
          action_item_->GetActionId().value());
    }
  }

  void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface&) override {
    controller_ = nullptr;
    observation_.Reset();
    action_item_subscription_ = {};
  }

  const raw_ref<PageActionContainerView> container_;
  const raw_ref<actions::ActionItem> action_item_;
  const PageActionProperties properties_;
  raw_ptr<PageActionController> controller_ = nullptr;
  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      observation_{this};
  base::CallbackListSubscription action_item_subscription_;
};

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageActionContainerView,
                                      kPageActionContainerViewElementId);

PageActionContainerView::PageActionContainerView(
    const std::vector<actions::ActionItem*>& action_items,
    const PageActionPropertiesProviderInterface& properties_provider,
    const PageActionViewParams& params)
    : view_params_(params) {
  SetProperty(views::kElementIdentifierKey, kPageActionContainerViewElementId);

  layout_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  layout_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Add `params.between_icon_spacing` dip after each child, except for the last
  // item.
  layout_
      ->SetDefault(views::kMarginsKey,
                   gfx::Insets().set_right(params.between_icon_spacing))
      .SetIgnoreDefaultMainAxisMargins(true);

  const bool lazy_creation = base::FeatureList::IsEnabled(
                                 features::kInitialWebUIChromeOptimizations) &&
                             features::kPageActionsLazyViewCreation.Get();
  size_t initial_index = 0;
  for (actions::ActionItem* action_item : action_items) {
    const auto action_item_id = action_item->GetActionId().value();
    const auto& properties = properties_provider.GetProperties(action_item_id);

    // Record the original index so suggestion chips or lazily materialized
    // views stay in their initial order.
    page_action_view_initial_indices_[action_item_id] = initial_index++;
    if (lazy_creation) {
      pending_actions_.try_emplace(action_item_id, *this, *action_item,
                                   properties);
    } else {
      CreatePageActionView(action_item, properties);
    }
  }
}

PageActionContainerView::~PageActionContainerView() = default;

void PageActionContainerView::SetController(PageActionController* controller) {
  for (const auto& [action_id, initial_index] :
       page_action_view_initial_indices_) {
    if (auto it = page_action_views_.find(action_id);
        it != page_action_views_.end()) {
      it->second->OnNewActiveController(controller);
    } else if (auto pending_it = pending_actions_.find(action_id);
               pending_it != pending_actions_.end()) {
      pending_it->second.SetController(controller);
    }
  }
}

PageActionView* PageActionContainerView::GetPageActionView(
    actions::ActionId action_id) {
  auto it = page_action_views_.find(action_id);
  return it != page_action_views_.end() ? it->second.get() : nullptr;
}

PageActionView* PageActionContainerView::EnsurePageActionView(
    actions::ActionId action_id) {
  if (auto* view = GetPageActionView(action_id)) {
    return view;
  }
  return pending_actions_.contains(action_id)
             ? MaterializePageActionView(action_id)
             : nullptr;
}

bool PageActionContainerView::IsPageActionVisible(
    actions::ActionId action_id) const {
  auto it = page_action_views_.find(action_id);
  return it != page_action_views_.end() && it->second->GetVisible();
}

PageActionView* PageActionContainerView::MaterializePageActionView(
    actions::ActionId action_id) {
  auto it = pending_actions_.find(action_id);
  CHECK(it != pending_actions_.end());
  actions::ActionItem* action_item = it->second.action_item();
  const PageActionProperties properties = it->second.properties();
  PageActionController* controller = it->second.controller();
  // Drop the placeholder before `PageActionView` attaches its own observer.
  pending_actions_.erase(it);

  PageActionView* view = CreatePageActionView(action_item, properties);
  if (controller) {
    view->OnNewActiveController(controller);
  }
  if (!view->GetDeclaredVisible()) {
    NormalizePageActionViewOrder();
  }
  return view;
}

PageActionView* PageActionContainerView::CreatePageActionView(
    actions::ActionItem* action_item,
    const PageActionProperties& properties) {
  PageActionView* view = AddChildView(std::make_unique<PageActionView>(
      action_item, view_params_, properties.type,
      properties.element_identifier));

  page_action_views_[action_item->GetActionId().value()] = view;
  auto on_state_changed =
      base::BindRepeating(&PageActionContainerView::OnPageActionStateChanged,
                          base::Unretained(this));
  page_action_state_changed_callbacks_.push_back(
      view->AddChipVisibilityChangedCallback(on_state_changed));
  page_action_state_changed_callbacks_.push_back(
      view->AddAnchoredMessageVisibilityChangedCallback(on_state_changed));
  page_action_state_changed_callbacks_.push_back(
      view->AddVisibilityChangedCallback(on_state_changed));

  view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          view_params_.hide_icon_on_space_constraint
              ? views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero
              : views::MinimumFlexSizeRule::kPreferredSnapToMinimum,
          views::MaximumFlexSizeRule::kPreferred));
  return view;
}

void PageActionContainerView::OnPageActionStateChanged(PageActionView* view) {
  NormalizePageActionViewOrder();
}

void PageActionContainerView::NormalizePageActionViewOrder() {
  // Three possible states of page actions: chip, icon, anchored message. There
  // can be multiple chips and/or icons, but at most one anchored message.
  std::vector<std::pair<size_t /*initial_index*/, PageActionView*>>
      chip_state_views;
  std::vector<std::pair<size_t /*initial_index*/, PageActionView*>>
      icon_state_views;
  std::optional<PageActionView*> anchored_message_state_view;

  chip_state_views.reserve(page_action_views_.size());
  icon_state_views.reserve(page_action_views_.size());

  for (const auto& [action_id, view] : page_action_views_) {
    const auto it = page_action_view_initial_indices_.find(action_id);
    CHECK(it != page_action_view_initial_indices_.end());
    if (view->IsAnchoredMessageVisible()) {
      anchored_message_state_view = view;
      continue;
    }

    const size_t initial_index = it->second;
    (view->IsChipVisible() ? chip_state_views : icon_state_views)
        .emplace_back(initial_index, view);
  }

  // Sort both groups by initial insertion index to keep stable, predictable
  // order.
  auto by_initial_index = [](const auto& a, const auto& b) {
    return a.first < b.first;
  };
  std::sort(chip_state_views.begin(), chip_state_views.end(), by_initial_index);
  std::sort(icon_state_views.begin(), icon_state_views.end(), by_initial_index);

  size_t next_index = 0;
  // Place the page action with an anchored message (if any) first.
  if (anchored_message_state_view) {
    ReorderChildView(anchored_message_state_view.value(), next_index++);
  }
  // Place all chips next, in initial-order.
  for (const auto& entry : chip_state_views) {
    ReorderChildView(entry.second, next_index++);
  }

  // Place the rest, offset by the number of chips.
  for (const auto& entry : icon_state_views) {
    ReorderChildView(entry.second, next_index++);
  }
  UpdateBackgroundAndMargins();
}

void PageActionContainerView::ChildVisibilityChanged(views::View* child) {
  UpdateBackgroundAndMargins();
}

// static
int PageActionContainerView::GetCapsuleHeight() {
  return GetLayoutConstant(LayoutConstant::kLocationBarHeight) -
         2 * GetLayoutConstant(LayoutConstant::kLocationBarElementPadding);
}

void PageActionContainerView::UpdateBackgroundAndMargins() {
  if (!features::IsPageActionsElevatedToolbarEnabled()) {
    return;
  }

  size_t visible_count = 0;
  for (views::View* child : children()) {
    auto* action_view = views::AsViewClass<PageActionView>(child);
    if (action_view && action_view->GetDeclaredVisible()) {
      visible_count++;
    }
  }

  const bool should_show_container = (visible_count > 1);
  if (!is_capsule_active_ && !should_show_container) {
    return;
  }

  if (should_show_container) {
    if (!is_capsule_active_ || GetBackground() == nullptr) {
      SetBackground(
          views::CreatePillBackground(ui::kColorSysBaseContainerElevated));
      layout_->SetInteriorMargin(gfx::Insets(kCapsuleInteriorPadding));
      layout_->SetDefault(views::kMarginsKey,
                          gfx::Insets::TLBR(0, 0, 0, kIconSpacing));
    }

    for (views::View* child : children()) {
      auto* action_view = views::AsViewClass<PageActionView>(child);
      if (action_view && action_view->GetDeclaredVisible() &&
          action_view->IsChipVisible()) {
        action_view->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(0, 0, 0, kChipSpacing));
      } else {
        child->ClearProperty(views::kMarginsKey);
      }
    }
  } else {
    SetBackground(nullptr);
    layout_->SetInteriorMargin(gfx::Insets());
    layout_->SetDefault(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0, view_params_.between_icon_spacing));
    for (views::View* child : children()) {
      child->ClearProperty(views::kMarginsKey);
    }
  }
  is_capsule_active_ = should_show_container;
}

bool PageActionContainerView::IsFirstVisibleViewChip() const {
  for (const views::View* child : children()) {
    if (const auto* action_view =
            views::AsViewClass<const PageActionView>(child)) {
      if (action_view->GetDeclaredVisible()) {
        return action_view->IsChipVisible();
      }
    }
  }
  return false;
}

BEGIN_METADATA(PageActionContainerView)
END_METADATA

}  // namespace page_actions
