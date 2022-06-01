// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"

namespace {

const gfx::VectorIcon& GetIcon(
    ExtensionsToolbarButton::ButtonType button_type) {
  return button_type == ExtensionsToolbarButton::ButtonType::kExtensions
             ? vector_icons::kExtensionIcon
             : kEyeIcon;
}

const std::u16string GetIconTooltip(
    ExtensionsToolbarButton::ButtonType button_type) {
  return l10n_util::GetStringUTF16(
      button_type == ExtensionsToolbarButton::ButtonType::kExtensions
          ? IDS_TOOLTIP_EXTENSIONS_BUTTON
          : IDS_TOOLTIP_EXTENSIONS_SITE_ACCESS_BUTTON);
}

}  // namespace

ExtensionsToolbarButton::ExtensionsToolbarButton(
    Browser* browser,
    ExtensionsToolbarContainer* extensions_container,
    ButtonType button_type)
    : ToolbarButton(PressedCallback()),
      browser_(browser),
      button_type_(button_type),
      extensions_container_(extensions_container) {
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

  SetTooltipText(GetIconTooltip(button_type_));
  SetVectorIcon(GetIcon(button_type_));

  if (button_type == ButtonType::kExtensions) {
    // Do not flip the Extensions icon in RTL.
    SetFlipCanvasOnPaintForRTLUI(false);
    SetID(VIEW_ID_EXTENSIONS_MENU_BUTTON);
  }

  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);
}

ExtensionsToolbarButton::~ExtensionsToolbarButton() {
  CHECK(!IsInObserverList());
}

gfx::Size ExtensionsToolbarButton::CalculatePreferredSize() const {
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

void ExtensionsToolbarButton::UpdateIcon() {
  if (browser_->app_controller()) {
    // TODO(pbos): Remove this once PWAs have ThemeProvider color support for it
    // and ToolbarButton can pick up icon sizes outside of a static lookup.
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      GetIcon(button_type_),
                      extensions_container_->GetIconColor(), GetIconSize()));
    return;
  }

  ToolbarButton::UpdateIcon();
}

void ExtensionsToolbarButton::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  pressed_lock_.reset();
  extensions_container_->OnMenuClosed();
}

void ExtensionsToolbarButton::ToggleExtensionsMenu() {
  if (ExtensionsTabbedMenuView::IsShowing()) {
    ExtensionsTabbedMenuView::Hide();
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
    menu = ExtensionsTabbedMenuView::ShowBubble(
        this, browser_, extensions_container_, button_type_,
        extensions_container_->CanShowIconInToolbar());
  } else {
    menu = ExtensionsMenuView::ShowBubble(
        this, browser_, extensions_container_,
        extensions_container_->CanShowIconInToolbar());
  }
  menu->AddObserver(this);
}

bool ExtensionsToolbarButton::GetExtensionsMenuShowing() const {
  return pressed_lock_.get();
}

int ExtensionsToolbarButton::GetIconSize() const {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  return (touch_ui && !browser_->app_controller()) ? kDefaultTouchableIconSize
                                                   : kDefaultIconSize;
}

BEGIN_METADATA(ExtensionsToolbarButton, ToolbarButton)
ADD_READONLY_PROPERTY_METADATA(bool, ExtensionsMenuShowing)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
