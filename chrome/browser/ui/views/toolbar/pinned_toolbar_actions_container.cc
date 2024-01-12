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
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/frame/browser_actions.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
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
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
const gfx::VectorIcon kEmptyIcon;

void RecordPinnedActionsCount(int count) {
  base::UmaHistogramCounts100("Browser.Actions.PinnedActionsCount", count);
}
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// PinnedToolbarActionsContainer::PinnedActionToolbarButton:

// TODO(b/299463180): Add right click context menus with an option for pinning
// unpinning.
PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    PinnedActionToolbarButton(Browser* browser,
                              actions::ActionId action_id,
                              PinnedToolbarActionsContainer* container)
    : ToolbarButton(
          base::BindRepeating(&PinnedActionToolbarButton::ButtonPressed,
                              base::Unretained(this)),
          CreateMenuModel(),
          nullptr,
          false),
      browser_(browser),
      action_item_(actions::ActionManager::Get().FindAction(
          action_id,
          BrowserActions::FromBrowser(browser)->root_action_item())),
      container_(container) {
  CHECK(action_item_);
  ConfigureInkDropForToolbar(this);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  set_drag_controller(container);
  GetViewAccessibility().OverrideDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);

  // Normally, the notify action is determined by whether a view is draggable
  // (and is set to press for non-draggable and release for draggable views).
  // However, PinnedActionToolbarButton may be draggable or non-draggable
  // depending on whether they are shown in an incognito window or unpinned and
  // popped-out. We want to preserve the same trigger event to keep the UX
  // (more) consistent. Set all PinnedActionToolbarButton to trigger on mouse
  // release.
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnRelease);

  // Do not flip the icon for RTL languages.
  SetFlipCanvasOnPaintForRTLUI(false);

  action_changed_subscription_ = action_item_->AddActionChangedCallback(
      base::BindRepeating(&PinnedToolbarActionsContainer::
                              PinnedActionToolbarButton::ActionItemChanged,
                          base::Unretained(this)));
  OnPropertyChanged(&action_item_, static_cast<views::PropertyEffects>(
                                       views::kPropertyEffectsLayout |
                                       views::kPropertyEffectsPaint));

  ActionItemChanged();
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    ~PinnedActionToolbarButton() = default;

actions::ActionId
PinnedToolbarActionsContainer::PinnedActionToolbarButton::GetActionId() {
  return *action_item_->GetActionId();
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::ButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("Actions.PinnedToolbarButtonActivation"));

  base::AutoReset<bool> invoking_action(&invoking_action_, true);
  action_item_->InvokeAction(
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kPinnedEntryToolbarButton))
          .Build());
}

bool PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    IsInvokingAction() {
  return invoking_action_;
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ToolbarButton::GetAccessibleNodeData(node_data);
  // TODO(shibalik): Revisit since all pinned actions should not be toggle
  // buttons.
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetCheckedState(IsActive() ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
}

bool PinnedToolbarActionsContainer::PinnedActionToolbarButton::IsActive() {
  return anchor_higlight_.has_value();
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::AddHighlight() {
  anchor_higlight_ = AddAnchorHighlight();
  if (pinned_) {
    NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged,
                             /*send_native_event=*/true);
  }
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    ResetHighlight() {
  anchor_higlight_.reset();
  if (pinned_) {
    NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged,
                             /*send_native_event=*/true);
  }
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    SetIconVisibility(bool visible) {
  if (action_item_->GetImage().IsVectorIcon()) {
    SetVectorIcon(visible
                      ? *action_item_->GetImage().GetVectorIcon().vector_icon()
                      : kEmptyIcon);
  } else {
    SetImageModel(views::Button::STATE_NORMAL,
                  visible ? action_item_->GetImage() : ui::ImageModel());
  }
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::SetPinned(
    bool pinned) {
  if (pinned_ == pinned) {
    return;
  }

  pinned_ = pinned;
  ActionItemChanged();
}

gfx::Size PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    CalculatePreferredSize() const {
  // This makes sure the buttons are at least the toolbar button sized width.
  // The preferred size might be smaller when the button's icon is removed
  // during drag/drop.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  const gfx::Size toolbar_button_size =
      browser_view
          ? browser_view->toolbar_button_provider()->GetToolbarButtonSize()
          : gfx::Size();
  const gfx::Size preferred_size = ToolbarButton::CalculatePreferredSize();
  return std::max(preferred_size, toolbar_button_size,
                  [](const gfx::Size s1, const gfx::Size s2) {
                    return s1.width() < s2.width();
                  });
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    ActionItemChanged() {
  auto tooltip_text = action_item_->GetTooltipText().empty()
                          ? action_item_->GetText()
                          : action_item_->GetTooltipText();
  SetTooltipText(tooltip_text);

  // Set the accessible name. Fall back to the tooltip if one is not provided.
  // If pinned, the pinned state is added to the accessible name.
  auto accessible_name = action_item_->GetAccessibleName().empty()
                             ? tooltip_text
                             : action_item_->GetAccessibleName();
  auto stateful_accessible_name =
      pinned_ ? l10n_util::GetStringFUTF16(
                    IDS_PINNED_ACTION_BUTTON_ACCESSIBLE_TITLE, accessible_name)
              : accessible_name;
  SetAccessibleName(stateful_accessible_name);

  // If possible use the vector icon so that it updates as the theme updates.
  if (action_item_->GetImage().IsVectorIcon()) {
    SetVectorIcon(*action_item_->GetImage().GetVectorIcon().vector_icon());
  } else {
    SetImageModel(views::Button::STATE_NORMAL, action_item_->GetImage());
  }
  SetEnabled(action_item_->GetEnabled());
  SetVisible(action_item_->GetVisible());
}

std::unique_ptr<ui::SimpleMenuModel>
PinnedToolbarActionsContainer::PinnedActionToolbarButton::CreateMenuModel() {
  std::unique_ptr<ui::SimpleMenuModel> model =
      std::make_unique<ui::SimpleMenuModel>(this);
  // String ID does not mean anything here as it is dynamic. It will get
  // recomputed  from `GetLabelForCommandId()`.
  model->AddItemWithStringId(IDC_UPDATE_SIDE_PANEL_PIN_STATE,
                             IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN);
  return model;
}

bool PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE;
}

std::u16string
PinnedToolbarActionsContainer::PinnedActionToolbarButton::GetLabelForCommandId(
    int command_id) const {
  if (command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE) {
    actions::ActionId action_id = action_item_->GetActionId().value();
    return l10n_util::GetStringUTF16(
        container_->IsActionPinned(action_id)
            ? IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN
            : IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN);
  }
  return std::u16string();
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::ExecuteCommand(
    int command_id,
    int event_flags) {
  if (command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE) {
    UpdatePinnedStateForContextMenu();
  }
}

bool PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    IsCommandIdEnabled(int command_id) const {
  if (command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE) {
    return browser_->profile()->IsRegularProfile() &&
           action_item_->GetProperty(actions::kActionItemPinnableKey);
  }
  return true;
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    UpdatePinnedStateForContextMenu() {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser_->profile());
  actions::ActionId action_id = action_item_->GetActionId().value();

  const bool updated_pin_state = !container_->IsActionPinned(action_id);
  const absl::optional<std::string> metrics_name =
      actions::ActionIdMap::ActionIdToString(action_id);
  CHECK(metrics_name.has_value());
  base::RecordComputedAction(
      base::StrCat({"Actions.PinnedToolbarButton.",
                    updated_pin_state ? "Pinned" : "Unpinned",
                    ".ByContextMenu.", metrics_name.value()}));
  // TODO(corising): Update the text for these notifications once pinning
  // expands past side panels.
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      updated_pin_state ? IDS_SIDE_PANEL_PINNED : IDS_SIDE_PANEL_UNPINNED));
  actions_model->UpdatePinnedState(action_id, updated_pin_state);
}

BEGIN_METADATA(PinnedToolbarActionsContainer,
               PinnedActionToolbarButton,
               ToolbarButton)
END_METADATA

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
    : browser_view_(browser_view),
      model_(PinnedToolbarActionsModel::Get(browser_view->GetProfile())) {
  SetProperty(views::kElementIdentifierKey,
              kPinnedToolbarActionsContainerElementId);
  // So we only get enter/exit messages when the mouse enters/exits the whole
  // container, even if it is entering/exiting a specific toolbar pinned
  // button view, too.
  SetNotifyEnterExitOnChild(true);

  model_observation_.Observe(model_.get());

  const int default_margin = GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN);
  const views::FlexSpecification hide_icon_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithWeight(0);

  auto* flex_layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse)
      .SetDefault(views::kFlexBehaviorKey,
                  hide_icon_flex_specification.WithOrder(1))
      .SetCollapseMargins(true)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, default_margin))
      .SetInteriorMargin(gfx::Insets());
  flex_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Create the toolbar divider.
  toolbar_divider_ = AddChildView(std::make_unique<views::View>());
  toolbar_divider_->SetProperty(views::kElementIdentifierKey,
                                kPinnedToolbarActionsContainerDividerElementId);
  toolbar_divider_->SetPreferredSize(
      gfx::Size(GetLayoutConstant(TOOLBAR_DIVIDER_WIDTH),
                GetLayoutConstant(TOOLBAR_DIVIDER_HEIGHT)));
  toolbar_divider_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, GetLayoutConstant(TOOLBAR_DIVIDER_SPACING)));
  toolbar_divider_->SetVisible(false);

  // Initialize the pinned action buttons.
  UpdateViews();
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
      button = AddPopOutButtonFor(id);
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
    button->SetProperty(views::kFlexBehaviorKey, views::FlexSpecification());
  } else {
    button->ResetHighlight();
    button->ClearProperty(views::kFlexBehaviorKey);
  }

  if (!pinned && !is_active) {
    RemovePoppedOutButtonFor(id);
  }

  UpdateDividerFlexSpecification();
  InvalidateLayout();
}

void PinnedToolbarActionsContainer::UpdateDividerFlexSpecification() {
  bool force_divider_visibility = false;
  for (auto* const pinned_button : pinned_buttons_) {
    if (pinned_button->IsActive()) {
      force_divider_visibility = true;
      break;
    }
  }

  if (force_divider_visibility) {
    toolbar_divider_->SetProperty(views::kFlexBehaviorKey,
                                  views::FlexSpecification());
  } else {
    toolbar_divider_->ClearProperty(views::kFlexBehaviorKey);
  }
  InvalidateLayout();
}

void PinnedToolbarActionsContainer::UpdateAllIcons() {
  for (auto* const pinned_button : pinned_buttons_) {
    pinned_button->UpdateIcon();
  }
}

void PinnedToolbarActionsContainer::OnThemeChanged() {
  const SkColor toolbar_divider_color =
      GetColorProvider()->GetColor(kColorToolbarExtensionSeparatorEnabled);
  toolbar_divider_->SetBackground(views::CreateRoundedRectBackground(
      toolbar_divider_color, GetLayoutConstant(TOOLBAR_DIVIDER_CORNER_RADIUS)));
  View::OnThemeChanged();
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
  absl::optional<actions::ActionId> action_id =
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

  const size_t visible_pinned_icons = pinned_buttons_.size();

  // Because the user can drag outside the container bounds, we need to clamp
  // to the valid range.
  before_icon = std::min(before_icon_unclamped, visible_pinned_icons - 1);

  if (!drop_info_.get() || drop_info_->index != before_icon) {
    drop_info_ = std::make_unique<DropInfo>(
        *actions::ActionIdMap::StringToActionId(data.id()), before_icon);
    SetActionButtonIconVisibility(drop_info_->action_id, false);
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

void PinnedToolbarActionsContainer::OnActionAdded(const actions::ActionId& id) {
  RecordPinnedActionsCount(model_->pinned_action_ids().size());
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void PinnedToolbarActionsContainer::OnActionRemoved(
    const actions::ActionId& id) {
  RecordPinnedActionsCount(model_->pinned_action_ids().size());
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void PinnedToolbarActionsContainer::OnActionMoved(const actions::ActionId& id,
                                                  int from_index,
                                                  int to_index) {
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
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
  DCHECK(iter != pinned_buttons_.end());
  auto* button = *iter;

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
      id, BrowserActions::FromBrowser(browser_view_->browser())
              ->root_action_item());
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton*
PinnedToolbarActionsContainer::AddPopOutButtonFor(const actions::ActionId& id) {
  CHECK(GetActionItemFor(id));
  auto popped_out_button = std::make_unique<PinnedActionToolbarButton>(
      browser_view_->browser(), id, this);
  auto* button = popped_out_button.get();
  popped_out_buttons_.push_back(AddChildView(std::move(popped_out_button)));
  ReorderViews();
  return button;
}

void PinnedToolbarActionsContainer::RemovePoppedOutButtonFor(
    const actions::ActionId& id) {
  const auto iter =
      base::ranges::find(popped_out_buttons_, id,
                         [](auto* button) { return button->GetActionId(); });
  if (iter == popped_out_buttons_.end()) {
    return;
  }
  RemoveButton(*iter);
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
    const auto iter =
        base::ranges::find(popped_out_buttons_, id,
                           [](auto* button) { return button->GetActionId(); });
    (*iter)->SetPinned(true);
    pinned_buttons_.push_back(*iter);
    popped_out_buttons_.erase(iter);
    // Flex specification of the divider might need to be updated when an active
    // button moves from popped out to pinned state.
    UpdateDividerFlexSpecification();
  } else {
    auto button = std::make_unique<PinnedActionToolbarButton>(
        browser_view_->browser(), id, this);
    button->SetPinned(true);
    pinned_buttons_.push_back(AddChildView(std::move(button)));
  }
}

void PinnedToolbarActionsContainer::RemovePinnedActionButtonFor(
    const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      pinned_buttons_, id, [](auto* button) { return button->GetActionId(); });
  if (iter == pinned_buttons_.end()) {
    return;
  }
  if (!(*iter)->IsActive()) {
    RemoveButton(*iter);
  } else {
    (*iter)->SetPinned(false);
    popped_out_buttons_.push_back(*iter);
  }
  pinned_buttons_.erase(iter);

  // Flex specification of the divider needs to be updated when an active pinned
  // button moves to popped out state.
  UpdateDividerFlexSpecification();
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton*
PinnedToolbarActionsContainer::GetPinnedButtonFor(const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      pinned_buttons_, id, [](auto* button) { return button->GetActionId(); });
  return iter == pinned_buttons_.end() ? nullptr : *iter;
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton*
PinnedToolbarActionsContainer::GetPoppedOutButtonFor(
    const actions::ActionId& id) {
  const auto iter =
      base::ranges::find(popped_out_buttons_, id,
                         [](auto* button) { return button->GetActionId(); });
  return iter == popped_out_buttons_.end() ? nullptr : *iter;
}

void PinnedToolbarActionsContainer::RemoveButton(
    PinnedActionToolbarButton* button) {
  if (button->IsInvokingAction()) {
    // Defer deletion of the view to allow the pressed event handler
    // that triggers its removal to run to completion.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, RemoveChildViewT(button));
  } else {
    RemoveChildViewT(button);
  }
}

bool PinnedToolbarActionsContainer::IsActionPinned(
    const actions::ActionId& id) {
  PinnedToolbarActionsContainer::PinnedActionToolbarButton* button =
      GetPinnedButtonFor(id);
  return button != nullptr;
}

bool PinnedToolbarActionsContainer::IsOverflowed(const actions::ActionId& id) {
  const auto* const pinned_button = GetPinnedButtonFor(id);
  // TODO(crbug.com/1508656): If this container is not visible treat the
  // elements inside as overflowed.
  // TODO(pengchaocai): Support popped out buttons overflow.
  return static_cast<views::LayoutManagerBase*>(GetLayoutManager())
             ->CanBeVisible(pinned_button) &&
         (!GetVisible() || !pinned_button->GetVisible());
}

void PinnedToolbarActionsContainer::ReorderViews() {
  size_t index = 0;
  // Pinned buttons appear first. Use the model's ordering of pinned ActionIds
  // because |pinned_buttons_| ordering is not updated on changes from the model
  // or from the user dragging to reorder.
  for (auto id : model_->pinned_action_ids()) {
    if (auto* button = GetPinnedButtonFor(id)) {
      ReorderChildView(button, index);
      index++;
    }
  }

  // Add the dragged button in its location if a drag is active.
  if (drop_info_.get()) {
    ReorderChildView(GetPinnedButtonFor(drop_info_->action_id),
                     drop_info_->index);
  }
  // The divider exist and is visible after the pinned buttons if any
  // exist.
  if (!pinned_buttons_.empty()) {
    toolbar_divider_->SetVisible(true);
    ReorderChildView(toolbar_divider_, index);
    index++;
  } else {
    toolbar_divider_->SetVisible(false);
  }
  // Popped out buttons appear last.
  for (auto* popped_out_button : popped_out_buttons_) {
    ReorderChildView(popped_out_button, index);
    index++;
  }
}

void PinnedToolbarActionsContainer::UpdateViews() {
  std::vector<actions::ActionId> old_ids;
  for (PinnedActionToolbarButton* const button : pinned_buttons_) {
    old_ids.push_back(button->GetActionId());
  }

  const std::vector<actions::ActionId>& new_ids = model_->pinned_action_ids();

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
  auto* button = GetPinnedButtonFor(id);
  if (!button) {
    return;
  }
  button->SetIconVisibility(visible);
}

void PinnedToolbarActionsContainer::MovePinnedAction(
    const actions::ActionId& action_id,
    size_t index,
    base::ScopedClosureRunner cleanup,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  model_->MovePinnedAction(action_id, index);

  output_drag_op = ui::mojom::DragOperation::kMove;
  // `cleanup` will run automatically when it goes out of scope to finish
  // up the drag.
}

void PinnedToolbarActionsContainer::DragDropCleanup(
    const actions::ActionId& dragged_action_id) {
  ReorderViews();
  SetActionButtonIconVisibility(dragged_action_id, true);
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
