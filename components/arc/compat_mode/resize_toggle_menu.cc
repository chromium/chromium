// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_toggle_menu.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "components/arc/compat_mode/resize_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

base::Optional<ResizeToggleMenu::CommandId> PredictCurrentMode(
    views::Widget* widget) {
  const int width = widget->GetWindowBoundsInScreen().width();
  const int height = widget->GetWindowBoundsInScreen().height();
  // We don't use the exact size here to predict tablet or phone size because
  // the window size might be bigger than it due to the ARC app-side minimum
  // size constraints.
  if (widget->IsMaximized())
    return ResizeToggleMenu::CommandId::kResizeDesktop;
  else if (width < height)
    return ResizeToggleMenu::CommandId::kResizePhone;
  else if (width > height)
    return ResizeToggleMenu::CommandId::kResizeTablet;
  return base::nullopt;
}

}  // namespace

ResizeToggleMenu::ResizeToggleMenu(views::Widget* widget,
                                   ArcResizeLockPrefDelegate* pref_delegate)
    : widget_(widget), pref_delegate_(pref_delegate) {
  model_ = MakeMenuModel();
  adapter_ = std::make_unique<views::MenuModelAdapter>(model_.get());
  root_view_ = adapter_->CreateMenu();

  const auto currentMode = PredictCurrentMode(widget_);
  if (currentMode) {
    auto* item = root_view_->GetMenuItemByID(*currentMode);
    item->SetSelected(true);
    item->SetMinorIcon(
        ui::ThemedVectorIcon(&ash::kHollowCheckCircleIcon,
                             ui::NativeTheme::kColorId_ProminentButtonColor));
  }

  menu_runner_ = std::make_unique<views::MenuRunner>(
      root_view_, views::MenuRunner::CONTEXT_MENU |
                      views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                      views::MenuRunner::FIXED_ANCHOR);

  const gfx::Rect client_view_rect =
      widget_->client_view()->GetBoundsInScreen();
  // Anchored to the right edge of the client_view.
  const gfx::Rect anchor_rect =
      gfx::Rect(client_view_rect.right(), client_view_rect.y(), 0,
                client_view_rect.height());
  menu_runner_->RunMenuAt(
      /*widget_owner=*/widget_, /*menu_button_controller=*/nullptr, anchor_rect,
      views::MenuAnchorPosition::kBubbleLeft, ui::MENU_SOURCE_MOUSE);
}

ResizeToggleMenu::~ResizeToggleMenu() {
  menu_runner_->Cancel();
}

std::unique_ptr<ui::SimpleMenuModel> ResizeToggleMenu::MakeMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);

  model->AddItemWithStringIdAndIcon(
      CommandId::kResizePhone, IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_PHONE,
      ui::ImageModel::FromVectorIcon(ash::kSystemMenuPhoneIcon));

  model->AddItemWithStringIdAndIcon(
      CommandId::kResizeTablet, IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_TABLET,
      ui::ImageModel::FromVectorIcon(ash::kSystemMenuTabletIcon));

  model->AddItemWithStringIdAndIcon(
      CommandId::kResizeDesktop, IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_DESKTOP,
      ui::ImageModel::FromVectorIcon(ash::kSystemMenuComputerIcon));

  model->AddSeparator(ui::NORMAL_SEPARATOR);

  model->AddItemWithStringIdAndIcon(
      CommandId::kOpenSettings,
      IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZE_SETTINGS,
      ui::ImageModel::FromVectorIcon(ash::kSystemMenuSettingsIcon));
  return model;
}

void ResizeToggleMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case CommandId::kResizePhone:
      ResizeToPhoneWithConfirmationIfNeeded(widget_, pref_delegate_);
      break;
    case CommandId::kResizeTablet:
      ResizeToTabletWithConfirmationIfNeeded(widget_, pref_delegate_);
      break;
    case CommandId::kResizeDesktop:
      ResizeToDesktopWithConfirmationIfNeeded(widget_, pref_delegate_);
      break;
    case CommandId::kOpenSettings:
      // TODO(b/181614585): Implement this.
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace arc
