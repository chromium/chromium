// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"

#include <string>
#include <type_traits>

#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container_layout.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
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
const gfx::VectorIcon kEmptyIcon;
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
    PinnedToolbarActionsContainer* container)
    : ToolbarButton(PressedCallback(), CreateMenuModel(), nullptr, false),
      browser_(browser),
      action_id_(action_id),
      container_(container) {
  SetProperty(views::kElementIdentifierKey,
              kPinnedActionToolbarButtonElementId);
  ConfigureInkDropForToolbar(this);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN)));
  set_drag_controller(container);
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

  // TODO(shibalik): Revisit since all pinned actions should not be toggle
  // buttons.
  GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
  GetViewAccessibility().SetCheckedState(ax::mojom::CheckedState::kFalse);
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
  constexpr int kModifiedFlag =
#if BUILDFLAG(IS_MAC)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif
  if (event.type() == ui::EventType::kKeyPressed &&
      (event.flags() & kModifiedFlag)) {
    const bool is_right = event.key_code() == ui::VKEY_RIGHT;
    const bool is_left = event.key_code() == ui::VKEY_LEFT;
    if (is_right || is_left) {
      const bool is_rtl = base::i18n::IsRTL();
      const bool is_next = (is_right && !is_rtl) || (is_left && is_rtl);
      if (pinned_ && browser_->profile()->IsRegularProfile()) {
        container_->MovePinnedActionBy(action_id_, is_next ? 1 : -1);
        return true;
      }
    }
  }
  return ToolbarButton::OnKeyPressed(event);
}

gfx::Size PinnedActionToolbarButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // This makes sure the buttons are at least the toolbar button sized width.
  // The preferred size might be smaller when the button's icon is removed
  // during drag/drop.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  const gfx::Size toolbar_button_size =
      browser_view
          ? browser_view->toolbar_button_provider()->GetToolbarButtonSize()
          : gfx::Size();
  const gfx::Size preferred_size =
      ToolbarButton::CalculatePreferredSize(available_size);
  return std::max(preferred_size, toolbar_button_size,
                  [](const gfx::Size s1, const gfx::Size s2) {
                    return s1.width() < s2.width();
                  });
}

void PinnedActionToolbarButton::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);
  gfx::Rect status_rect(14, 2);
  status_indicator_->SetColorId(kColorToolbarActionItemEngaged,
                                kColorToolbarButtonIconInactive);

  gfx::Rect image_container_bounds = image_container_view()->GetLocalBounds();
  int new_x = image_container_bounds.x() +
              (image_container_bounds.width() - status_rect.width()) / 2;
  int new_y = image_container_bounds.bottom() + 1;
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
  if (!icons.has_value()) {
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

std::unique_ptr<ui::SimpleMenuModel>
PinnedActionToolbarButton::CreateMenuModel() {
  std::unique_ptr<ui::SimpleMenuModel> model =
      std::make_unique<ui::SimpleMenuModel>(this);
  // String ID and icon do not mean anything here as it is dynamic. It will get
  // recomputed  from `GetLabelForCommandId()` and `GetIconForCommandId`.
  model->AddItemWithStringIdAndIcon(
      IDC_UPDATE_SIDE_PANEL_PIN_STATE,
      IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN,
      ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon, 16));
  if (features::IsToolbarPinningEnabled()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringIdAndIcon(
        IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR, IDS_SHOW_CUSTOMIZE_CHROME_TOOLBAR,
        ui::ImageModel::FromVectorIcon(kSettingsMenuIcon, ui::kColorIcon, 16));
  }
  return model;
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

bool PinnedActionToolbarButton::IsItemForCommandIdDynamic(
    int command_id) const {
  return command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE;
}

std::u16string PinnedActionToolbarButton::GetLabelForCommandId(
    int command_id) const {
  if (command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE) {
    return l10n_util::GetStringUTF16(
        container_->IsActionPinned(action_id_)
            ? IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN
            : IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN);
  }
  return std::u16string();
}

ui::ImageModel PinnedActionToolbarButton::GetIconForCommandId(
    int command_id) const {
  if (command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE) {
    return ui::ImageModel::FromVectorIcon(pinned_ ? kKeepOffIcon : kKeepIcon,
                                          ui::kColorIcon, 16);
  }
  return ui::ImageModel();
}

void PinnedActionToolbarButton::ExecuteCommand(int command_id,
                                               int event_flags) {
  if (command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE) {
    UpdatePinnedStateForContextMenu();
  } else if (command_id == IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR) {
    chrome::ExecuteCommand(browser_, IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR);
  }
}

bool PinnedActionToolbarButton::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDC_UPDATE_SIDE_PANEL_PIN_STATE) {
    return browser_->profile()->IsRegularProfile() && is_pinnable_;
  }
  if (command_id == IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR) {
    tabs::TabModel* tab = browser_->tab_strip_model()->GetActiveTab();
    customize_chrome::SidePanelController* side_panel_controller =
        tab->tab_features()->customize_chrome_side_panel_controller();
    return side_panel_controller &&
           side_panel_controller->IsCustomizeChromeEntryAvailable();
  }
  return true;
}

void PinnedActionToolbarButton::UpdatePinnedStateForContextMenu() {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser_->profile());

  const bool updated_pin_state = !container_->IsActionPinned(action_id_);
  const std::optional<std::string> metrics_name =
      actions::ActionIdMap::ActionIdToString(action_id_);
  CHECK(metrics_name.has_value());
  base::RecordComputedAction(
      base::StrCat({"Actions.PinnedToolbarButton.",
                    updated_pin_state ? "Pinned" : "Unpinned",
                    ".ByContextMenu.", metrics_name.value()}));
  if (features::IsToolbarPinningEnabled()) {
    GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
        updated_pin_state ? IDS_TOOLBAR_BUTTON_PINNED
                          : IDS_TOOLBAR_BUTTON_UNPINNED));
  } else {
    GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
        updated_pin_state ? IDS_SIDE_PANEL_PINNED : IDS_SIDE_PANEL_UNPINNED));
  }
  actions_model->UpdatePinnedState(action_id_, updated_pin_state);
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

  // Update whether the action is engaged before updating the view.
  action_view_->SetActionEngaged(
      action_item->GetProperty(kActionItemUnderlineIndicatorKey));

  OnViewChangedImpl(action_item);
  action_view_->SetIsPinnable(
      action_item->GetProperty(actions::kActionItemPinnableKey));
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
  CHECK(metrics_name.has_value());
  base::RecordComputedAction(base::StrCat(
      {"Actions.PinnedToolbarButtonActivation.", metrics_name.value()}));

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
  // Update the button's icon.
  if (action_item->GetImage().IsVectorIcon()) {
    action_view_->SetVectorIcon(
        action_view_->IsIconVisible()
            ? *action_item->GetImage().GetVectorIcon().vector_icon()
            : kEmptyIcon);
  } else {
    action_view_->SetImageModel(views::Button::STATE_NORMAL,
                                action_view_->IsIconVisible()
                                    ? action_item->GetImage()
                                    : ui::ImageModel());
  }
  // Set the accessible name. Fall back to the tooltip if one is not provided.
  // If pinned, the pinned state is added to the accessible name.
  auto accessible_name = action_item->GetAccessibleName().empty()
                             ? action_view_->GetTooltipText(gfx::Point())
                             : action_item->GetAccessibleName();
  auto stateful_accessible_name =
      action_view_->IsPinned()
          ? l10n_util::GetStringFUTF16(
                IDS_PINNED_ACTION_BUTTON_ACCESSIBLE_TITLE, accessible_name)
          : accessible_name;
  action_view_->GetViewAccessibility().SetName(stateful_accessible_name);
  action_view_->UpdateStatusIndicator();
}

BEGIN_METADATA(PinnedActionToolbarButton)
END_METADATA
