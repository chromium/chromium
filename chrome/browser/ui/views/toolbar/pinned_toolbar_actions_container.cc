// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container_layout.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/action_id.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
void RecordPinnedActionsCount(int count) {
  base::UmaHistogramCounts100("Browser.Actions.PinnedActionsCount", count);
}
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// PinnedToolbarActionsContainer::DropInfo:

struct PinnedToolbarActionsContainer::DropInfo {
  explicit DropInfo(actions::ActionId action_id, size_t index);

  // The id for the action being dragged.
  actions::ActionId action_id;

  // The (0-indexed) index the action will be dropped.
  size_t index;
};

PinnedToolbarActionsContainer::DropInfo::DropInfo(actions::ActionId action_id,
                                                  size_t index)
    : action_id(action_id), index(index) {}

///////////////////////////////////////////////////////////////////////////////
// PinnedToolbarActionsContainer:

PinnedToolbarActionsContainer::PinnedToolbarActionsContainer(
    BrowserView* browser_view)
    : ToolbarIconContainerView(/*uses_highlight=*/false,
                               /*use_default_target_layout=*/false),
      browser_view_(browser_view),
      model_(PinnedToolbarActionsModel::Get(browser_view->GetProfile())) {
  if (features::IsToolbarPinningEnabled()) {
    SetPaintToLayer();
  }
  SetProperty(views::kElementIdentifierKey,
              kPinnedToolbarActionsContainerElementId);
  // So we only get enter/exit messages when the mouse enters/exits the whole
  // container, even if it is entering/exiting a specific toolbar pinned
  // button view, too.
  SetNotifyEnterExitOnChild(true);

  model_observation_.Observe(model_.get());
  const views::FlexSpecification hide_icon_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithWeight(0);

  PinnedToolbarActionsContainerLayout* layout =
      GetAnimatingLayoutManager()->SetTargetLayoutManager(
          std::make_unique<PinnedToolbarActionsContainerLayout>());
  // Set the interior margins to ensure the default margins are negated if there
  // is a button at the end of the container or if the divider is at the end
  // (which has a different margin than the default). This ensures the container
  // is the same size regardless of where and if the divider is in the
  // container.
  layout->SetInteriorMargin(
      gfx::Insets::VH(0, -GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN)));
  if (features::IsToolbarPinningEnabled()) {
    GetAnimatingLayoutManager()->SetDefaultFadeMode(
        views::AnimatingLayoutManager::FadeInOutMode::
            kFadeAndSlideFromTrailingEdge);
    GetAnimatingLayoutManager()->SetTweenType(
        gfx::Tween::Type::FAST_OUT_SLOW_IN_3);
    GetAnimatingLayoutManager()->SetAnimationDuration(base::Milliseconds(300));
    GetAnimatingLayoutManager()->SetOpacityTweenType(gfx::Tween::Type::LINEAR);
    GetAnimatingLayoutManager()->SetOpacityAnimationDuration(
        base::Milliseconds(200));
  }

  // Create the toolbar divider.
  std::unique_ptr<views::View> toolbar_divider =
      std::make_unique<views::View>();
  toolbar_divider->SetProperty(views::kElementIdentifierKey,
                               kPinnedToolbarActionsContainerDividerElementId);
  toolbar_divider->SetPreferredSize(
      gfx::Size(GetLayoutConstant(TOOLBAR_DIVIDER_WIDTH),
                GetLayoutConstant(TOOLBAR_DIVIDER_HEIGHT)));
  toolbar_divider->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, GetLayoutConstant(TOOLBAR_DIVIDER_SPACING)));
  toolbar_divider_ = AddChildView(std::move(toolbar_divider));

  // Initialize the pinned action buttons.
  action_view_controller_ = std::make_unique<views::ActionViewController>();

  // Before creating the pinned buttons, verify that the pref value is correct
  // and update it if not. If the user has been moved into a different default
  // pin state group (i.e. from the default being false to the default being
  // true) we want to make sure their pin state changes if they have not
  // explicitly changed it themselves.
  if (SearchCompanionSidePanelCoordinator::IsSupported(
          browser_view_->GetProfile(),
          /*include_runtime_checks=*/false) &&
      browser_view_->GetProfile()->GetPrefs()) {
    companion::UpdateCompanionDefaultPinnedToToolbarState(
        browser_view_->GetProfile());
  }

  model_->MaybeMigrateChromeLabsPinnedState();

  UpdateViews();
}

int PinnedToolbarActionsContainer::CalculatePoppedOutButtonsWidth() {
  if (popped_out_buttons_.empty()) {
    return 0;
  }

  int popped_out_buttons_width = 0;

  for (PinnedActionToolbarButton* const popped_button : popped_out_buttons_) {
    popped_out_buttons_width += popped_button->GetPreferredSize().width();
  }

  popped_out_buttons_width += (popped_out_buttons_.size() - 1) *
                              (GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN));

  return popped_out_buttons_width;
}

gfx::Size PinnedToolbarActionsContainer::DefaultFlexRule(
    const views::SizeBounds& size_bounds) {
  // Get the default flex rule
  auto default_flex_rule = GetAnimatingLayoutManager()->GetDefaultFlexRule();

  // Calculate the size according to the default flex rule
  return default_flex_rule.Run(this, size_bounds);
}

PinnedToolbarActionsContainer::~PinnedToolbarActionsContainer() = default;

void PinnedToolbarActionsContainer::UpdateActionState(actions::ActionId id,
                                                      bool is_active) {
  auto* button = GetPinnedButtonFor(id);
  bool pinned = button != nullptr;

  // Get or create popped out button if not pinned.
  if (!pinned) {
    button = GetPoppedOutButtonFor(id);
    if (!button && is_active) {
      button = AddPoppedOutButtonFor(id);
    }
  }
  // If the button doesn't exist, do nothing. This could happen if |is_active|
  // is false and there is no existing pinned out popped out button for the
  // |id|.
  if (!button) {
    return;
  }

  // Update button highlight and force visibility if the button is active.
  if (is_active) {
    button->AddHighlight();
  } else {
    button->ResetHighlight();
  }

  if (!is_active) {
    MaybeRemovePoppedOutButtonFor(id);
  }

  InvalidateLayout();
}

void PinnedToolbarActionsContainer::ShowActionEphemerallyInToolbar(
    actions::ActionId id,
    bool show) {
  auto* button = GetButtonFor(id);
  // If the button doesn't exist and shouldn't be shown, do nothing.
  if (!button && !show) {
    return;
  }
  // Create the button if it doesn't exist.
  if (!button) {
    button = AddPoppedOutButtonFor(id);
  }
  button->SetShouldShowEphemerallyInToolbar(show);

  if (!show) {
    MaybeRemovePoppedOutButtonFor(id);
  }
}

void PinnedToolbarActionsContainer::MovePinnedActionBy(actions::ActionId id,
                                                       int delta) {
  DCHECK(IsActionPinned(id));
  const auto& pinned_action_ids = model_->PinnedActionIds();

  auto iter = base::ranges::find(pinned_action_ids, id);
  CHECK(iter != pinned_action_ids.end());

  int current_index = std::distance(pinned_action_ids.begin(), iter);
  int target_index = current_index + delta;

  if (target_index >= 0 && target_index < int(pinned_action_ids.size())) {
    model_->MovePinnedAction(id, target_index);
  }
}

void PinnedToolbarActionsContainer::UpdateAllIcons() {
  for (PinnedActionToolbarButton* const pinned_button : pinned_buttons_) {
    pinned_button->UpdateIcon();
  }
}

void PinnedToolbarActionsContainer::OnThemeChanged() {
  const SkColor toolbar_divider_color =
      GetColorProvider()->GetColor(kColorToolbarExtensionSeparatorEnabled);
  toolbar_divider_->SetBackground(views::CreateRoundedRectBackground(
      toolbar_divider_color, GetLayoutConstant(TOOLBAR_DIVIDER_CORNER_RADIUS)));
  ToolbarIconContainerView::OnThemeChanged();
}

bool PinnedToolbarActionsContainer::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool PinnedToolbarActionsContainer::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool PinnedToolbarActionsContainer::CanDrop(const OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(data,
                                        browser_view_->browser()->profile());
}

void PinnedToolbarActionsContainer::OnDragEntered(
    const ui::DropTargetEvent& event) {
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

int PinnedToolbarActionsContainer::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data())) {
    return ui::DragDropTypes::DRAG_NONE;
  }

  // Check if the action item for the dragged icon is pinned (e.g. an action
  // item could be unpinned through a sync update while dragging its icon).
  std::optional<actions::ActionId> action_id =
      actions::ActionIdMap::StringToActionId(data.id());
  if (!action_id.has_value() || !model_->Contains(*action_id)) {
    return ui::DragDropTypes::DRAG_NONE;
  }

  size_t before_icon = 0;
  // Figure out where to display the icon during dragging transition.

  // First, since we want to update the dragged action's position from before
  // an icon to after it when the event passes the midpoint between two icons.
  // This will convert the event coordinate into the index of the icon we want
  // to display the dragged action before. We also mirror the event.x() so
  // that our calculations are consistent with left-to-right.
  // Note we are not including popped-out icons here, only the pinned actions.
  const int offset_into_icon_area = GetMirroredXInView(event.x());
  const size_t before_icon_unclamped = WidthToIconCount(offset_into_icon_area);

  const size_t visible_pinned_icons = base::ranges::count_if(
      pinned_buttons_,
      [](PinnedActionToolbarButton* button) { return button->GetVisible(); });
  const size_t button_offset = pinned_buttons_.size() - visible_pinned_icons;

  // Because the user can drag outside the container bounds, we need to clamp
  // to the valid range.
  before_icon =
      std::min(before_icon_unclamped, visible_pinned_icons - 1) + button_offset;

  if (!drop_info_.get() || drop_info_->index != before_icon) {
    drop_info_ = std::make_unique<DropInfo>(
        *actions::ActionIdMap::StringToActionId(data.id()), before_icon);
    if (auto* button = GetPinnedButtonFor(drop_info_->action_id)) {
      button->SetIconVisibility(false);
    }
    ReorderViews();
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

void PinnedToolbarActionsContainer::OnDragExited() {
  if (!drop_info_) {
    return;
  }

  const actions::ActionId dragged_action_id = drop_info_->action_id;
  drop_info_.reset();
  DragDropCleanup(dragged_action_id);
}

views::View::DropCallback PinnedToolbarActionsContainer::GetDropCallback(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data())) {
    return base::NullCallback();
  }

  auto action_id = drop_info_->action_id;
  auto index = drop_info_->index;
  drop_info_.reset();
  base::ScopedClosureRunner cleanup(
      base::BindOnce(&PinnedToolbarActionsContainer::DragDropCleanup,
                     weak_ptr_factory_.GetWeakPtr(), action_id));
  return base::BindOnce(&PinnedToolbarActionsContainer::MovePinnedAction,
                        drop_weak_ptr_factory_.GetWeakPtr(), action_id, index,
                        std::move(cleanup));
}

void PinnedToolbarActionsContainer::OnActionAddedLocally(
    const actions::ActionId& id) {
  RecordPinnedActionsCount(model_->PinnedActionIds().size());
}

void PinnedToolbarActionsContainer::OnActionRemovedLocally(
    const actions::ActionId& id) {
  RecordPinnedActionsCount(model_->PinnedActionIds().size());
}

void PinnedToolbarActionsContainer::OnActionsChanged() {
  UpdateViews();
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void PinnedToolbarActionsContainer::WriteDragDataForView(
    View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  DCHECK(data);

  const auto iter = base::ranges::find(pinned_buttons_, sender);
  CHECK(iter != pinned_buttons_.end(), base::NotFatalUntil::M130);
  auto* button = (*iter).get();

  ui::ImageModel icon =
      ui::ImageModel::FromImageSkia(button->GetImage(button->GetState()));
  data->provider().SetDragImage(icon.Rasterize(GetColorProvider()),
                                press_pt.OffsetFromOrigin());

  // Fill in the remaining info.
  size_t index = iter - pinned_buttons_.begin();
  BrowserActionDragData drag_data(
      *actions::ActionIdMap::ActionIdToString(button->GetActionId()), index);
  drag_data.Write(browser_view_->GetProfile(), data);
}

int PinnedToolbarActionsContainer::GetDragOperationsForView(
    View* sender,
    const gfx::Point& p) {
  return browser_view_->GetProfile()->IsOffTheRecord()
             ? ui::DragDropTypes::DRAG_NONE
             : ui::DragDropTypes::DRAG_MOVE;
}

bool PinnedToolbarActionsContainer::CanStartDragForView(
    View* sender,
    const gfx::Point& press_pt,
    const gfx::Point& p) {
  // We don't allow dragging buttons that aren't pinned, or if
  // the profile is incognito (to avoid changing state from an incognito
  // window).
  const auto iter = base::ranges::find(pinned_buttons_, sender);
  return iter != pinned_buttons_.end() &&
         !browser_view_->GetProfile()->IsOffTheRecord();
}

actions::ActionItem* PinnedToolbarActionsContainer::GetActionItemFor(
    const actions::ActionId& id) {
  return actions::ActionManager::Get().FindAction(
      id, browser_view_->browser()->browser_actions()->root_action_item());
}

PinnedActionToolbarButton* PinnedToolbarActionsContainer::AddPoppedOutButtonFor(
    const actions::ActionId& id) {
  CHECK(GetActionItemFor(id));
  auto popped_out_button = std::make_unique<PinnedActionToolbarButton>(
      browser_view_->browser(), id, this);
  auto* button = popped_out_button.get();
  action_view_controller_->CreateActionViewRelationship(
      button, GetActionItemFor(id)->GetAsWeakPtr());
  if (features::IsToolbarPinningEnabled()) {
    popped_out_button->SetPaintToLayer();
    popped_out_button->layer()->SetFillsBoundsOpaquely(false);
  }
  popped_out_buttons_.push_back(AddChildView(std::move(popped_out_button)));
  ReorderViews();
  return button;
}

void PinnedToolbarActionsContainer::MaybeRemovePoppedOutButtonFor(
    const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      popped_out_buttons_, id,
      [](PinnedActionToolbarButton* button) { return button->GetActionId(); });
  if (iter == popped_out_buttons_.end() ||
      ShouldRemainPoppedOutInToolbar(*iter)) {
    return;
  }
  GetAnimatingLayoutManager()->FadeOut(*iter);
  GetAnimatingLayoutManager()->PostOrQueueAction(
      base::BindOnce(&PinnedToolbarActionsContainer::RemoveButton,
                     weak_ptr_factory_.GetWeakPtr(), *iter));
  popped_out_buttons_.erase(iter);
  ReorderViews();
}

void PinnedToolbarActionsContainer::AddPinnedActionButtonFor(
    const actions::ActionId& id) {
  actions::ActionItem* action_item = GetActionItemFor(id);
  // If the action item doesn't exist (i.e. a new id synced from an
  // update-to-date device to an out-of-date device) we do not want to create a
  // toolbar button for it.
  if (!action_item) {
    return;
  }
  if (GetPoppedOutButtonFor(id)) {
    const auto iter = base::ranges::find(popped_out_buttons_, id,
                                         [](PinnedActionToolbarButton* button) {
                                           return button->GetActionId();
                                         });
    (*iter)->SetPinned(true);
    pinned_buttons_.push_back(*iter);
    popped_out_buttons_.erase(iter);
  } else {
    auto button = std::make_unique<PinnedActionToolbarButton>(
        browser_view_->browser(), id, this);
    action_view_controller_->CreateActionViewRelationship(
        button.get(), action_item->GetAsWeakPtr());
    button->SetPinned(true);
    if (features::IsToolbarPinningEnabled()) {
      button->SetPaintToLayer();
      button->layer()->SetFillsBoundsOpaquely(false);
    }
    pinned_buttons_.push_back(AddChildView(std::move(button)));
  }
}

void PinnedToolbarActionsContainer::RemovePinnedActionButtonFor(
    const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      pinned_buttons_, id,
      [](PinnedActionToolbarButton* button) { return button->GetActionId(); });
  if (iter == pinned_buttons_.end()) {
    return;
  }
  if (!ShouldRemainPoppedOutInToolbar(*iter)) {
    GetAnimatingLayoutManager()->FadeOut(*iter);
    GetAnimatingLayoutManager()->PostOrQueueAction(
        base::BindOnce(&PinnedToolbarActionsContainer::RemoveButton,
                       weak_ptr_factory_.GetWeakPtr(), *iter));
  } else {
    (*iter)->SetPinned(false);
    popped_out_buttons_.push_back(*iter);
  }
  pinned_buttons_.erase(iter);
}

PinnedActionToolbarButton* PinnedToolbarActionsContainer::GetPinnedButtonFor(
    const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      pinned_buttons_, id,
      [](PinnedActionToolbarButton* button) { return button->GetActionId(); });
  return iter == pinned_buttons_.end() ? nullptr : *iter;
}

PinnedActionToolbarButton* PinnedToolbarActionsContainer::GetPoppedOutButtonFor(
    const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      popped_out_buttons_, id,
      [](PinnedActionToolbarButton* button) { return button->GetActionId(); });
  return iter == popped_out_buttons_.end() ? nullptr : *iter;
}

PinnedActionToolbarButton* PinnedToolbarActionsContainer::GetButtonFor(
    const actions::ActionId& id) {
  if (auto* button = GetPinnedButtonFor(id)) {
    return button;
  }
  return GetPoppedOutButtonFor(id);
}

bool PinnedToolbarActionsContainer::ShouldRemainPoppedOutInToolbar(
    PinnedActionToolbarButton* button) {
  return button->IsActive() || button->ShouldShowEphemerallyInToolbar();
}

void PinnedToolbarActionsContainer::RemoveButton(
    PinnedActionToolbarButton* button) {
  if (button->NeedsDelayedDestruction()) {
    // Defer deletion of the view to allow the pressed event handler
    // that triggers its removal to run to completion.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, RemoveChildViewT(button));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PinnedToolbarActionsContainer::InvalidateLayout,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    RemoveChildViewT(button);
    InvalidateLayout();
  }
}

bool PinnedToolbarActionsContainer::IsOverflowed(const actions::ActionId& id) {
  const auto* const pinned_button = GetPinnedButtonFor(id);
  // TODO(pengchaocai): Support popped out buttons overflow.
  // TODO(crbug.com/40949386): If this container is not visible treat the
  // elements inside as overflowed.

  // Need to use the target layout in case the animation has not yet shown the
  // button but is in the process of revealing it.
  const auto* const layout =
      GetAnimatingLayoutManager()->target_layout().GetLayoutFor(pinned_button);
  return GetAnimatingLayoutManager()->target_layout_manager()->CanBeVisible(
             pinned_button) &&
         layout && (!GetVisible() || !layout->visible);
}

views::View* PinnedToolbarActionsContainer::GetContainerView() {
  return static_cast<views::View*>(this);
}

bool PinnedToolbarActionsContainer::ShouldAnyButtonsOverflow(
    gfx::Size available_size) const {
  views::ProposedLayout proposed_layout;
  if (GetAnimatingLayoutManager()->is_animating()) {
    proposed_layout = GetAnimatingLayoutManager()->target_layout();
  } else {
    proposed_layout =
        GetAnimatingLayoutManager()->target_layout_manager()->GetProposedLayout(
            available_size);
  }
  for (PinnedActionToolbarButton* pinned_button : pinned_buttons_) {
    if (views::ChildLayout* child_layout =
            proposed_layout.GetLayoutFor(pinned_button)) {
      if (GetAnimatingLayoutManager()->target_layout_manager()->CanBeVisible(
              pinned_button) &&
          !child_layout->visible) {
        return true;
      }
    }
  }
  return false;
}

bool PinnedToolbarActionsContainer::IsActionPinned(
    const actions::ActionId& id) {
  PinnedActionToolbarButton* button = GetPinnedButtonFor(id);
  return button != nullptr;
}

bool PinnedToolbarActionsContainer::IsActionPoppedOut(
    const actions::ActionId& id) {
  PinnedActionToolbarButton* button = GetPoppedOutButtonFor(id);
  return button != nullptr;
}

bool PinnedToolbarActionsContainer::IsActionPinnedOrPoppedOut(
    const actions::ActionId& id) {
  return IsActionPinned(id) || IsActionPoppedOut(id);
}

void PinnedToolbarActionsContainer::ReorderViews() {
  size_t index = 0;
  // Pinned buttons appear first. Use the model's ordering of pinned ActionIds
  // because |pinned_buttons_| ordering is not updated on changes from the model
  // or from the user dragging to reorder.
  const auto ordered_pinned_ids = model_->PinnedActionIds();
  for (auto id : ordered_pinned_ids) {
    if (auto* button = GetPinnedButtonFor(id)) {
      ReorderChildView(button, index);
      index++;
    }
  }

  // Update the order of |pinned_buttons_| to reflect the model's order.
  std::sort(pinned_buttons_.begin(), pinned_buttons_.end(),
            [ordered_pinned_ids](PinnedActionToolbarButton* button_1,
                                 PinnedActionToolbarButton* button_2) {
              int button_1_index =
                  std::find(ordered_pinned_ids.begin(),
                            ordered_pinned_ids.end(), button_1->GetActionId()) -
                  ordered_pinned_ids.begin();
              int button_2_index =
                  std::find(ordered_pinned_ids.begin(),
                            ordered_pinned_ids.end(), button_2->GetActionId()) -
                  ordered_pinned_ids.begin();
              return button_1_index < button_2_index;
            });

  // Add the dragged button in its location if a drag is active.
  if (drop_info_.get()) {
    ReorderChildView(GetPinnedButtonFor(drop_info_->action_id),
                     drop_info_->index);
  }
  // The divider exist and is after the pinned buttons if any
  // exist.
  ReorderChildView(toolbar_divider_, index);
  index++;

  // Popped out buttons appear last.
  for (PinnedActionToolbarButton* popped_out_button : popped_out_buttons_) {
    ReorderChildView(popped_out_button, index);
    index++;
  }
}

void PinnedToolbarActionsContainer::UpdateViews() {
  std::vector<actions::ActionId> old_ids;
  for (PinnedActionToolbarButton* const button : pinned_buttons_) {
    old_ids.push_back(button->GetActionId());
  }

  const std::vector<actions::ActionId>& new_ids = model_->PinnedActionIds();

  // 1. Remove buttons for actions in the UI that are not present in the
  // model.
  for (const actions::ActionId& id : old_ids) {
    if (base::Contains(new_ids, id)) {
      continue;
    }

    // End the drag session if the dragged button is being removed.
    if (drop_info_ && drop_info_->action_id == id) {
      drop_info_.reset();
    }

    RemovePinnedActionButtonFor(id);
  }

  // 2. Add buttons for actions that are in the model but not in the UI.
  for (const actions::ActionId& id : new_ids) {
    if (base::Contains(old_ids, id)) {
      continue;
    }

    AddPinnedActionButtonFor(id);
  }

  // 3. Clamp the drag index within the new bounds of the container in cases
  // where a button was removed by sync while a user was dragging a different
  // button.
  if (drop_info_.get() && drop_info_->index >= pinned_buttons_.size()) {
    drop_info_->index = std::max(size_t(0), pinned_buttons_.size() - 1);
  }

  // 4. Ensure the views match the ordering in the model.
  ReorderViews();
}

void PinnedToolbarActionsContainer::SetActionButtonIconVisibility(
    actions::ActionId id,
    bool visible) {
  if (auto* button = GetPinnedButtonFor(id)) {
    button->SetIconVisibility(visible);
  }
}

void PinnedToolbarActionsContainer::MovePinnedAction(
    const actions::ActionId& action_id,
    size_t index,
    base::ScopedClosureRunner cleanup,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  // Adjust the index based on the the the button we are moving it next to in
  // the toolbar. This is necessary because there might be ids in the model that
  // are not currently available that need to be factored into the index
  // calculation.
  if (index != pinned_buttons_.size() - 1) {
    auto target_index_button = pinned_buttons_[index];
    const auto& pinned_action_ids = model_->PinnedActionIds();
    auto it = find(pinned_action_ids.begin(), pinned_action_ids.end(),
                   target_index_button->GetActionId());
    CHECK(it != pinned_action_ids.end());
    index = it - pinned_action_ids.begin();
  }
  model_->MovePinnedAction(action_id, index);

  output_drag_op = ui::mojom::DragOperation::kMove;
  // `cleanup` will run automatically when it goes out of scope to finish
  // up the drag.
}

void PinnedToolbarActionsContainer::DragDropCleanup(
    const actions::ActionId& dragged_action_id) {
  ReorderViews();
  GetAnimatingLayoutManager()->PostOrQueueAction(base::BindOnce(
      &PinnedToolbarActionsContainer::SetActionButtonIconVisibility,
      weak_ptr_factory_.GetWeakPtr(), dragged_action_id, true));
}

size_t PinnedToolbarActionsContainer::WidthToIconCount(int x_offset) {
  const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  size_t unclamped_count = std::max(
      (x_offset + element_padding) / (browser_view_->toolbar_button_provider()
                                          ->GetToolbarButtonSize()
                                          .width() +
                                      element_padding),
      0);
  return std::min(unclamped_count, pinned_buttons_.size());
}

BEGIN_METADATA(PinnedToolbarActionsContainer)
END_METADATA
