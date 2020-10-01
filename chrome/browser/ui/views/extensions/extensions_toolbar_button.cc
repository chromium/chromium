// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"

ExtensionsToolbarButton::ExtensionsToolbarButton(
    Browser* browser,
    ExtensionsToolbarContainer* extensions_container)
    : ToolbarButton(nullptr),
      browser_(browser),
      extensions_container_(extensions_container) {
  std::unique_ptr<views::MenuButtonController> menu_button_controller =
      std::make_unique<views::MenuButtonController>(
          this,
          base::BindRepeating(&ExtensionsToolbarButton::ButtonPressed,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_EXTENSIONS_BUTTON));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
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

const char* ExtensionsToolbarButton::GetClassName() const {
  return "ExtensionsToolbarButton";
}

void ExtensionsToolbarButton::UpdateIcon() {
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    vector_icons::kExtensionIcon,
                    extensions_container_->GetIconColor(), GetIconSize()));
}

void ExtensionsToolbarButton::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  pressed_lock_.reset();
}

int ExtensionsToolbarButton::GetIconSize() const {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  return (touch_ui && !browser_->app_controller()) ? kDefaultTouchableIconSize
                                                   : kDefaultIconSize;
}

void ExtensionsToolbarButton::ButtonPressed() {
  if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return;
  }
  pressed_lock_ = menu_button_controller_->TakeLock();
  base::RecordAction(base::UserMetricsAction("Extensions.Toolbar.MenuOpened"));
  ExtensionsMenuView::ShowBubble(this, browser_, extensions_container_,
                                 extensions_container_->CanShowIconInToolbar())
      ->AddObserver(this);
}
