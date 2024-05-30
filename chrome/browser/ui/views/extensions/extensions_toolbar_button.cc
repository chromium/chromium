// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"

namespace {

const gfx::VectorIcon& GetIcon(ExtensionsToolbarButton::State state) {
  switch (state) {
    case ExtensionsToolbarButton::State::kDefault:
      return vector_icons::kExtensionChromeRefreshIcon;
    case ExtensionsToolbarButton::State::kAllExtensionsBlocked:
      return vector_icons::kExtensionOffIcon;
    case ExtensionsToolbarButton::State::kAnyExtensionHasAccess:
      return vector_icons::kExtensionOnIcon;
  }
}

// Returns the accessible text for the button.
std::u16string GetAccessibleText(ExtensionsToolbarButton::State state) {
  int message_id;
  switch (state) {
    case ExtensionsToolbarButton::State::kDefault:
      message_id = IDS_ACC_NAME_EXTENSIONS_BUTTON;
      break;
    case ExtensionsToolbarButton::State::kAllExtensionsBlocked:
      message_id = IDS_ACC_NAME_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED;
      break;
    case ExtensionsToolbarButton::State::kAnyExtensionHasAccess:
      message_id = IDS_ACC_NAME_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS;
      break;
  }
  return l10n_util::GetStringUTF16(message_id);
}

}  // namespace

ExtensionsToolbarButton::ExtensionsToolbarButton(
    Browser* browser,
    ExtensionsToolbarContainer* extensions_container,
    ExtensionsMenuCoordinator* extensions_menu_coordinator)
    : ToolbarChipButton(PressedCallback()),
      browser_(browser),
      extensions_container_(extensions_container),
      extensions_menu_coordinator_(extensions_menu_coordinator) {
  std::unique_ptr<views::MenuButtonController> menu_button_controller =
      std::make_unique<views::MenuButtonController>(
          this,
          base::BindRepeating(&ExtensionsToolbarButton::ToggleExtensionsMenu,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  SetVectorIcon(GetIcon(state_));

  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);

  // Do not flip the Extensions icon in RTL.
  SetFlipCanvasOnPaintForRTLUI(false);
  SetID(VIEW_ID_EXTENSIONS_MENU_BUTTON);

  // Set button for IPH.
  SetProperty(views::kElementIdentifierKey, kExtensionsMenuButtonElementId);

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    GetViewAccessibility().SetName(GetAccessibleText(state_));
    // By default, the button's accessible description is set to the button's
    // tooltip text. This is the accepted workaround to ensure only accessible
    // name is announced by a screenreader rather than tooltip text and
    // accessible name.
    GetViewAccessibility().SetDescription(
        std::u16string(),
        ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  } else {
    // We need to set the tooltip at construction when it's used by the
    // accessibility mode.
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_EXTENSIONS_BUTTON));
  }
}

ExtensionsToolbarButton::~ExtensionsToolbarButton() {
  CHECK(!IsInObserverList());
}

gfx::Size ExtensionsToolbarButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return extensions_container_->GetToolbarActionSize();
}

gfx::Size ExtensionsToolbarButton::GetMinimumSize() const {
  const int icon_size = GetIconSize();
  gfx::Size min_size(icon_size, icon_size);
  min_size.SetToMin(GetPreferredSize());
  return min_size;
}

void ExtensionsToolbarButton::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // Because this button is in a container and doesn't necessarily take up the
  // whole height of the toolbar, the standard insets calculation does not
  // apply. Instead calculate the insets as the difference between the icon
  // size and the preferred button size.

  const gfx::Size current_size = size();
  if (current_size.IsEmpty())
    return;
  const int icon_size = GetIconSize();
  gfx::Insets new_insets;
  if (icon_size < current_size.width()) {
    const int diff = current_size.width() - icon_size;
    new_insets.set_left(diff / 2);
    new_insets.set_right((diff + 1) / 2);
  }
  if (icon_size < current_size.height()) {
    const int diff = current_size.height() - icon_size;
    new_insets.set_top(diff / 2);
    new_insets.set_bottom((diff + 1) / 2);
  }
  SetLayoutInsets(new_insets);
}

void ExtensionsToolbarButton::UpdateState(State state) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  if (state == state_) {
    return;
  }

  state_ = state;
  SetVectorIcon(GetIcon(state_));
  GetViewAccessibility().SetName(GetAccessibleText(state_));
}

void ExtensionsToolbarButton::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  pressed_lock_.reset();
  extensions_container_->OnMenuClosed();
}

bool ExtensionsToolbarButton::ShouldShowInkdropAfterIphInteraction() {
  return false;
}

void ExtensionsToolbarButton::ToggleExtensionsMenu() {
  if (extensions_menu_coordinator_ &&
      extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
    return;
  } else if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return;
  }

  pressed_lock_ = menu_button_controller_->TakeLock();
  extensions_container_->OnMenuOpening();
  base::RecordAction(base::UserMetricsAction("Extensions.Toolbar.MenuOpened"));
  views::Widget* menu;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    if (extensions_container_->GetRequestAccessButton()->GetVisible()) {
      base::RecordAction(base::UserMetricsAction(
          "Extensions.Toolbar.MenuOpenedWhenExtensionsAreRequestingAccess"));
    }
    extensions_menu_coordinator_->Show(this, extensions_container_);
    menu = extensions_menu_coordinator_->GetExtensionsMenuWidget();
  } else {
    menu =
        ExtensionsMenuView::ShowBubble(this, browser_, extensions_container_);
  }
  menu->AddObserver(this);
}

bool ExtensionsToolbarButton::GetExtensionsMenuShowing() const {
  return pressed_lock_.get();
}

int ExtensionsToolbarButton::GetIconSize() const {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  if (touch_ui && !browser_->app_controller()) {
    return kDefaultTouchableIconSize;
  }

  return kDefaultIconSizeChromeRefresh;
}

std::u16string ExtensionsToolbarButton::GetTooltipText(
    const gfx::Point& p) const {
  int message_id;
  switch (state_) {
    case ExtensionsToolbarButton::State::kDefault:
      message_id = IDS_TOOLTIP_EXTENSIONS_BUTTON;
      break;
    case ExtensionsToolbarButton::State::kAllExtensionsBlocked:
      message_id = IDS_TOOLTIP_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED;
      break;
    case ExtensionsToolbarButton::State::kAnyExtensionHasAccess:
      message_id = IDS_TOOLTIP_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS;
      break;
  }
  return l10n_util::GetStringUTF16(message_id);
}

BEGIN_METADATA(ExtensionsToolbarButton)
ADD_READONLY_PROPERTY_METADATA(bool, ExtensionsMenuShowing)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
