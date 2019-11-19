// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"

ExtensionsToolbarButton::ExtensionsToolbarButton(
    Browser* browser,
    ExtensionsToolbarContainer* extensions_container)
    : ToolbarButton(this),
      browser_(browser),
      extensions_container_(extensions_container) {
  std::unique_ptr<views::MenuButtonController> menu_button_controller =
      std::make_unique<views::MenuButtonController>(
          this, this,
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_EXTENSIONS_BUTTON));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);
}

void ExtensionsToolbarButton::UpdateIcon() {
  const int icon_size = ui::MaterialDesignController::touch_ui()
                            ? kDefaultTouchableIconSize
                            : kDefaultIconSize;
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kExtensionIcon, icon_size,
                                 extensions_container_->GetIconColor()));
}

void ExtensionsToolbarButton::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return;
  }
  ExtensionsMenuView::ShowBubble(this, browser_, extensions_container_);
}
