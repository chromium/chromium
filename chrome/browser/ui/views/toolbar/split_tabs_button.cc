// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"

#include <memory>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"

namespace {
// Width of the status indicator shown across the button.
constexpr int kStatusIndicatorWidth = 14;
// Height of the status indicator shown across the button.
constexpr int kStatusIndicatorHeight = 2;
// Spacing between the button's icon and the status indicator.
constexpr int kStatusIndicatorSpacing = 1;
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabsToolbarButton,
                                      kUpdatePinStateMenu);

SplitTabsToolbarButton::SplitTabsToolbarButton(Browser* browser)
    : ToolbarButton(
          views::Button::PressedCallback(),
          std::make_unique<PinnedActionToolbarButtonMenuModel>(browser,
                                                               kActionSplitTab),
          nullptr,
          false),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey,
              kToolbarSplitTabsToolbarButtonElementId);
  set_menu_identifier(kUpdatePinStateMenu);
  SetButtonController(std::make_unique<views::MenuButtonController>(
      this,
      base::BindRepeating(&SplitTabsToolbarButton::ButtonPressed,
                          base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(this)));
  pin_state_.Init(
      prefs::kPinSplitTabButton, browser_->profile()->GetPrefs(),
      base::BindRepeating(&SplitTabsToolbarButton::UpdateButtonVisibility,
                          base::Unretained(this)));
  views::View* const image_container = image_container_view();
  status_indicator_ =
      PinnedToolbarButtonStatusIndicator::Install(image_container);
  status_indicator_->SetColorId(kColorToolbarActionItemEngaged,
                                kColorToolbarButtonIconInactive);
  UpdateButtonVisibility();
  split_tab_menu_ = std::make_unique<SplitTabMenuModel>(
      browser_->tab_strip_model(),
      SplitTabMenuModel::MenuSource::kToolbarButton);
  browser->tab_strip_model()->AddObserver(this);
}

SplitTabsToolbarButton::~SplitTabsToolbarButton() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void SplitTabsToolbarButton::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    UpdateButtonVisibility();
  }
}

void SplitTabsToolbarButton::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kAdded ||
      change.type == SplitTabChange::Type::kRemoved ||
      change.type == SplitTabChange::Type::kContentsChanged) {
    UpdateButtonVisibility();
  }
}

void SplitTabsToolbarButton::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);
  gfx::Rect status_rect(kStatusIndicatorWidth, kStatusIndicatorHeight);
  const gfx::Rect image_container_bounds =
      image_container_view()->GetLocalBounds();
  const int new_x =
      image_container_bounds.x() +
      (image_container_bounds.width() - kStatusIndicatorWidth) / 2;
  const int new_y = image_container_bounds.bottom() + kStatusIndicatorSpacing;
  status_rect.set_origin(gfx::Point(new_x, new_y));
  status_indicator_->SetBoundsRect(status_rect);
}

void SplitTabsToolbarButton::UpdateIcon() {
  const std::optional<VectorIcons>& icons = GetVectorIcons();
  if (!icons.has_value() || !GetColorProvider()) {
    return;
  }

  if (IsActiveTabInSplit()) {
    const gfx::VectorIcon& icon = ui::TouchUiController::Get()->touch_ui()
                                      ? icons->touch_icon
                                      : icons->icon;
    SkColor engaged_color =
        GetColorProvider()->GetColor(kColorToolbarActionItemEngaged);
    UpdateIconsWithColors(icon, engaged_color, engaged_color, engaged_color,
                          GetForegroundColor(ButtonState::STATE_DISABLED));
  } else {
    ToolbarButton::UpdateIcon();
  }
}

const std::optional<ToolbarButton::VectorIcons>&
SplitTabsToolbarButton::GetIconsForTesting() {
  return GetVectorIcons();
}

bool SplitTabsToolbarButton::IsActiveTabInSplit() {
  TabStripModel* const tab_strip_model = browser_->tab_strip_model();
  return tab_strip_model && tab_strip_model->GetActiveTab() &&
         tab_strip_model->GetActiveTab()->IsSplit();
}

void SplitTabsToolbarButton::ButtonPressed(const ui::Event& event) {
  if (IsActiveTabInSplit()) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        split_tab_menu_.get(), views::MenuRunner::HAS_MNEMONICS);
    menu_runner_->RunMenuAt(
        GetWidget(),
        static_cast<views::MenuButtonController*>(button_controller()),
        GetAnchorBoundsInScreen(), views::MenuAnchorPosition::kTopLeft,
        ui::GetMenuSourceTypeForEvent(event));
  } else {
    chrome::NewSplitTab(browser_,
                        split_tabs::SplitTabCreatedSource::kToolbarButton);
  }
}

void SplitTabsToolbarButton::UpdateButtonVisibility() {
  const bool is_active_tab_in_split = IsActiveTabInSplit();
  UpdateButtonIcon();
  UpdateStatusIndicator(is_active_tab_in_split);
  SetVisible(pin_state_.GetValue() || is_active_tab_in_split);
  UpdateAccessibilityRole(is_active_tab_in_split);
  UpdateAccessibilityLabel(is_active_tab_in_split);
}

void SplitTabsToolbarButton::UpdateButtonIcon() {
  TabStripModel* const tab_strip_model = browser_->tab_strip_model();
  tabs::TabInterface* const active_tab = tab_strip_model->GetActiveTab();
  if (active_tab && active_tab->IsSplit()) {
    const split_tabs::SplitTabActiveLocation location =
        split_tabs::GetLastActiveTabLocation(tab_strip_model,
                                             active_tab->GetSplit().value());
    constexpr auto icons =
        base::MakeFixedFlatMap<split_tabs::SplitTabActiveLocation,
                               const gfx::VectorIcon*>({
            {split_tabs::SplitTabActiveLocation::kStart, &kSplitSceneLeftIcon},
            {split_tabs::SplitTabActiveLocation::kEnd, &kSplitSceneRightIcon},
            {split_tabs::SplitTabActiveLocation::kTop, &kSplitSceneUpIcon},
            {split_tabs::SplitTabActiveLocation::kBottom, &kSplitSceneDownIcon},
        });
    SetVectorIcon(*icons.at(location));
  } else {
    SetVectorIcon(kSplitSceneIcon);
  }
}

void SplitTabsToolbarButton::UpdateStatusIndicator(bool show_status_indicator) {
  if (show_status_indicator) {
    status_indicator_->Show();
  } else {
    status_indicator_->Hide();
  }
}

void SplitTabsToolbarButton::UpdateAccessibilityRole(bool has_menu) {
  auto role =
      has_menu ? ax::mojom::Role::kPopUpButton : ax::mojom::Role::kButton;

  if (role == GetViewAccessibility().GetCachedRole()) {
    return;
  }

  GetViewAccessibility().SetRole(role);
  GetViewAccessibility().SetHasPopup(has_menu ? ax::mojom::HasPopup::kMenu
                                              : ax::mojom::HasPopup::kFalse);
}

void SplitTabsToolbarButton::UpdateAccessibilityLabel(bool is_enabled) {
  auto string_id = is_enabled ? IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_ENABLED
                              : IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_PINNED;

  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(string_id));
  SetTooltipText(l10n_util::GetStringUTF16(string_id));
}

BEGIN_METADATA(SplitTabsToolbarButton)
END_METADATA
