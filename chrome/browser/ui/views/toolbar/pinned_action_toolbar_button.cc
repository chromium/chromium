// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"

#include <string>
#include <type_traits>

#include "base/auto_reset.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container_layout.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
// Width of the status indicator shown across the button.
constexpr int kStatusIndicatorWidth = 14;
// Height of the status indicator shown across the button.
constexpr int kStatusIndicatorHeight = 2;
// Spacing between the button's icon and the status indicator.
constexpr int kStatusIndicatorSpacing = 1;
}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(PinnedToolbarActionFlexPriority)
DEFINE_UI_CLASS_PROPERTY_KEY(
    std::underlying_type_t<PinnedToolbarActionFlexPriority>,
    kToolbarButtonFlexPriorityKey,
    std::underlying_type_t<PinnedToolbarActionFlexPriority>(
        PinnedToolbarActionFlexPriority::kLow))

PinnedActionToolbarButton::PinnedActionToolbarButton(
    Browser* browser,
    actions::ActionId action_id,
    base::WeakPtr<PinnedToolbarActionsContainer> container)
    : ToolbarButton(
          PressedCallback(),
          std::make_unique<PinnedActionToolbarButtonMenuModel>(browser,
                                                               action_id),
          nullptr,
          false),
      browser_(browser),
      action_id_(action_id),
      container_(container) {
  SetProperty(views::kElementIdentifierKey,
              kPinnedActionToolbarButtonElementId);
  ConfigureInkDropForToolbar(this);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  // Pinned action toolbar buttons have right margin and no left margin.
  SetProperty(views::kMarginsKey,
              gfx::Insets::TLBR(
                  0, 0, 0, GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN)));
  set_drag_controller(container_.get());
  GetViewAccessibility().SetDescription(
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
  action_count_changed_subscription_ = AddAnchorCountChangedCallback(
      base::BindRepeating(&PinnedActionToolbarButton::OnAnchorCountChanged,
                          base::Unretained(this)));

  status_indicator_ =
      PinnedToolbarButtonStatusIndicator::Install(image_container_view());
  status_indicator_->SetColorId(kColorToolbarActionItemEngaged,
                                kColorToolbarButtonIconInactive);

  // TODO(shibalik): Revisit since all pinned actions should not be toggle
  // buttons.
  GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
  GetViewAccessibility().SetCheckedState(ax::mojom::CheckedState::kFalse);

  if (web_app::AppBrowserController::IsWebApp(browser_)) {
    SetLayoutInsets(gfx::Insets());
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetAppearDisabledInInactiveWidget(true);
  }
}

PinnedActionToolbarButton::~PinnedActionToolbarButton() {
  action_count_changed_subscription_ = {};
}

bool PinnedActionToolbarButton::IsActive() {
  return anchor_higlight_.has_value();
}

base::AutoReset<bool> PinnedActionToolbarButton::SetNeedsDelayedDestruction(
    bool needs_delayed_destruction) {
  return base::AutoReset<bool>(&needs_delayed_destruction_,
                               needs_delayed_destruction);
}

void PinnedActionToolbarButton::SetIconVisibility(bool is_visible) {
  is_icon_visible_ = is_visible;
  NotifyViewControllerCallback();
}

void PinnedActionToolbarButton::AddHighlight() {
  anchor_higlight_ = AddAnchorHighlight();
  GetViewAccessibility().SetCheckedState(ax::mojom::CheckedState::kTrue);
}

void PinnedActionToolbarButton::ResetHighlight() {
  anchor_higlight_.reset();
  GetViewAccessibility().SetCheckedState(ax::mojom::CheckedState::kFalse);
}

void PinnedActionToolbarButton::SetPinned(bool pinned) {
  if (pinned_ == pinned) {
    return;
  }
  pinned_ = pinned;
  NotifyViewControllerCallback();
}

bool PinnedActionToolbarButton::OnKeyPressed(const ui::KeyEvent& event) {
  std::optional<event_utils::ReorderDirection> reorder_direction =
      event_utils::GetReorderCommandForKeyboardEvent(event);
  if (reorder_direction && pinned_ && browser_->profile()->IsRegularProfile()) {
    int move_by = 0;
    switch (*reorder_direction) {
      case event_utils::ReorderDirection::kPrevious:
        move_by = -1;
        break;
      case event_utils::ReorderDirection::kNext:
        move_by = 1;
        break;
    }

    container_->MovePinnedActionBy(action_id_, move_by);
    return true;
  }

  return ToolbarButton::OnKeyPressed(event);
}

gfx::Size PinnedActionToolbarButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // This makes sure the buttons are at least the toolbar button sized width.
  // The preferred size might be smaller when the button's icon is removed
  // during drag/drop.
  if (!container_) {
    // Want to avoid this ever getting called during teardown.
    return gfx::Size();
  }
  const gfx::Size toolbar_button_size = container_->GetDefaultButtonSize();
  const gfx::Size preferred_size =
      ToolbarButton::CalculatePreferredSize(available_size);
  return std::max(preferred_size, toolbar_button_size,
                  [](const gfx::Size s1, const gfx::Size s2) {
                    return s1.width() < s2.width();
                  });
}

void PinnedActionToolbarButton::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);
  gfx::Rect status_rect(kStatusIndicatorWidth, kStatusIndicatorHeight);
  const gfx::Rect image_container_bounds =
      image_container_view()->GetLocalBounds();
  const int new_x = image_container_bounds.x() +
                    (image_container_bounds.width() - status_rect.width()) / 2;
  const int new_y = image_container_bounds.bottom() + kStatusIndicatorSpacing;
  // Set the new origin for status_rect
  status_rect.set_origin(gfx::Point(new_x, new_y));
  status_indicator_->SetBoundsRect(status_rect);
}

bool PinnedActionToolbarButton::OnMousePressed(const ui::MouseEvent& event) {
  skip_execution_ = is_action_showing_bubble_;
  return ToolbarButton::OnMousePressed(event);
}

void PinnedActionToolbarButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (!skip_execution_) {
    ToolbarButton::OnMouseReleased(event);
  } else {
    OnClickCanceled(event);
  }
  skip_execution_ = false;
}

void PinnedActionToolbarButton::UpdateIcon() {
  const std::optional<VectorIcons>& icons = GetVectorIcons();
  // If the button is a cached permanent button the color provider will not be
  // available.
  if (!icons.has_value() || !GetColorProvider()) {
    return;
  }

  const gfx::VectorIcon& icon = ui::TouchUiController::Get()->touch_ui()
                                    ? icons->touch_icon
                                    : icons->icon;

  if (is_icon_visible_ && action_engaged_) {
    UpdateIconsWithColors(
        icon, GetColorProvider()->GetColor(kColorToolbarActionItemEngaged),
        GetColorProvider()->GetColor(kColorToolbarActionItemEngaged),
        GetColorProvider()->GetColor(kColorToolbarActionItemEngaged),
        GetForegroundColor(ButtonState::STATE_DISABLED));
  } else {
    UpdateIconsWithColors(icon, GetForegroundColor(ButtonState::STATE_NORMAL),
                          GetForegroundColor(ButtonState::STATE_HOVERED),
                          GetForegroundColor(ButtonState::STATE_PRESSED),
                          GetForegroundColor(ButtonState::STATE_DISABLED));
  }
}

bool PinnedActionToolbarButton::ShouldShowEphemerallyInToolbar() {
  return should_show_in_toolbar_ || has_anchor_;
}

void PinnedActionToolbarButton::SetActionEngaged(bool action_engaged) {
  if (!IsActive()) {
    SetProperty(
        kToolbarButtonFlexPriorityKey,
        action_engaged
            ? static_cast<
                  std::underlying_type_t<PinnedToolbarActionFlexPriority>>(
                  PinnedToolbarActionFlexPriority::kMedium)
            : static_cast<
                  std::underlying_type_t<PinnedToolbarActionFlexPriority>>(
                  PinnedToolbarActionFlexPriority::kLow));
    InvalidateLayout();
  }
  action_engaged_ = action_engaged;
}

void PinnedActionToolbarButton::HideStatusIndicator() {
  status_indicator_->Hide();
}

void PinnedActionToolbarButton::UpdateStatusIndicator() {
  if (action_engaged_ && is_icon_visible_) {
    status_indicator_->Show();
  } else {
    status_indicator_->Hide();
  }
}

void PinnedActionToolbarButton::OnAnchorCountChanged(size_t anchor_count) {
  // If there is something anchored to the button we want to make sure the
  // button will be visible in the toolbar in cases where the window might be
  // small enough that icons must overflow. Update the
  // kToolbarButtonFlexPriorityKey to make sure icons are forced visible or able
  // to overflow.
  if (anchor_count > 0) {
    SetProperty(
        kToolbarButtonFlexPriorityKey,
        static_cast<std::underlying_type_t<PinnedToolbarActionFlexPriority>>(
            PinnedToolbarActionFlexPriority::kHigh));
    InvalidateLayout();
    has_anchor_ = true;
  } else {
    SetProperty(
        kToolbarButtonFlexPriorityKey,
        action_engaged_
            ? static_cast<
                  std::underlying_type_t<PinnedToolbarActionFlexPriority>>(
                  PinnedToolbarActionFlexPriority::kMedium)
            : static_cast<
                  std::underlying_type_t<PinnedToolbarActionFlexPriority>>(
                  PinnedToolbarActionFlexPriority::kLow));
    InvalidateLayout();
    has_anchor_ = false;
    container_->MaybeRemovePoppedOutButtonFor(GetActionId());
  }
}

std::unique_ptr<views::ActionViewInterface>
PinnedActionToolbarButton::GetActionViewInterface() {
  return std::make_unique<PinnedActionToolbarButtonActionViewInterface>(this);
}

PinnedActionToolbarButtonActionViewInterface::
    PinnedActionToolbarButtonActionViewInterface(
        PinnedActionToolbarButton* action_view)
    : ToolbarButtonActionViewInterface(action_view),
      action_view_(action_view) {}

void PinnedActionToolbarButtonActionViewInterface::ActionItemChangedImpl(
    actions::ActionItem* action_item) {
  ButtonActionViewInterface::ActionItemChangedImpl(action_item);

  if (action_view_->IsIconVisible() &&
      actions::IsActionItemClass<actions::StatefulImageActionItem>(
          action_item)) {
    auto* stateful_action_item =
        static_cast<actions::StatefulImageActionItem*>(action_item);
    if (stateful_action_item->GetStatefulImage().IsVectorIcon()) {
      action_view_->SetVectorIcon(*stateful_action_item->GetStatefulImage()
                                       .GetVectorIcon()
                                       .vector_icon());
    }
  }

  // Update whether the action is engaged before updating the view.
  action_view_->SetActionEngaged(
      action_item->GetProperty(kActionItemUnderlineIndicatorKey));

  bool is_pinnable = true;
  switch (static_cast<actions::ActionPinnableState>(
      action_item->GetProperty(actions::kActionItemPinnableKey))) {
    case actions::ActionPinnableState::kNotPinnable:
      is_pinnable = false;
      break;
    case actions::ActionPinnableState::kPinnable:
    case actions::ActionPinnableState::kEnterpriseControlled:
      is_pinnable = true;
      break;
    default:
      NOTREACHED();
  }

  if (!is_pinnable && action_view_->IsPinned()) {
    action_view_->SetVisible(false);
  }

  OnViewChangedImpl(action_item);

  action_view_->SetIsActionShowingBubble(action_item->GetIsShowingBubble());
}

void PinnedActionToolbarButtonActionViewInterface::InvokeActionImpl(
    actions::ActionItem* action_item) {
  base::RecordAction(
      base::UserMetricsAction("Actions.PinnedToolbarButtonActivation"));
  std::optional<actions::ActionId> action_id = action_item->GetActionId();
  CHECK(action_id.has_value());
  const std::optional<std::string> metrics_name =
      actions::ActionIdMap::ActionIdToString(action_id.value());
  // ActionIdToStringMappings are not initialized in unit tests, therefore will
  // not have a value. In the normal case, `metrics_name` should always have a
  // value.
  if (metrics_name.has_value()) {
    base::RecordComputedAction(base::StrCat(
        {"Actions.PinnedToolbarButtonActivation.", metrics_name.value()}));
  }

  base::AutoReset<bool> needs_delayed_destruction =
      action_view_->SetNeedsDelayedDestruction(true);
  action_item->InvokeAction(
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kPinnedEntryToolbarButton))
          .Build());
}

void PinnedActionToolbarButtonActionViewInterface::OnViewChangedImpl(
    actions::ActionItem* action_item) {
  // Update the button's icon. If the action item is a stateful image action
  // item, use the stateful image. Otherwise, use the action item's image.
  ui::ImageModel image_model;

  if (actions::IsActionItemClass<actions::StatefulImageActionItem>(
          action_item)) {
    image_model = static_cast<actions::StatefulImageActionItem*>(action_item)
                      ->GetStatefulImage();
  } else {
    image_model = action_item->GetImage();
  }

  if (image_model.IsVectorIcon()) {
    action_view_->SetVectorIcon(action_view_->IsIconVisible()
                                    ? *image_model.GetVectorIcon().vector_icon()
                                    : gfx::VectorIcon::EmptyIcon());
  } else {
    action_view_->SetImageModel(
        views::Button::STATE_NORMAL,
        action_view_->IsIconVisible() ? image_model : ui::ImageModel());
  }
  // Set the accessible name. Fall back to the tooltip if one is not provided.
  // If pinned, the pinned state is added to the accessible name.
  const std::u16string accessible_name(action_item->GetAccessibleName().empty()
                                           ? action_view_->GetTooltipText()
                                           : action_item->GetAccessibleName());
  action_view_->GetViewAccessibility().SetName(
      action_view_->IsPinned()
          ? l10n_util::GetStringFUTF16(
                IDS_PINNED_ACTION_BUTTON_ACCESSIBLE_TITLE, accessible_name)
          : accessible_name);
  action_view_->UpdateStatusIndicator();
}

BEGIN_METADATA(PinnedActionToolbarButton)
END_METADATA
