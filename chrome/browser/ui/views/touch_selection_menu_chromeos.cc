// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/touch_selection_menu_chromeos.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/touch_selection_menu_runner_chromeos.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

namespace {

constexpr size_t kSmallIconSizeInDip = 16;

}  // namespace

TouchSelectionMenuChromeOS::TouchSelectionMenuChromeOS(
    views::TouchSelectionMenuRunnerViews* owner,
    ui::TouchSelectionMenuClient* client,
    aura::Window* context,
    arc::mojom::TextSelectionActionPtr action)
    : views::TouchSelectionMenuViews(owner, client, context),
      action_(std::move(action)),
      display_id_(
          display::Screen::GetScreen()->GetDisplayNearestWindow(context).id()) {
}

void TouchSelectionMenuChromeOS::SetActionsForTesting(
    std::vector<arc::mojom::TextSelectionActionPtr> actions) {
  action_ = std::move(actions[0]);

  // Since we are forcing new button entries here, it is very likely that the
  // default action buttons are already added, we should remove the existent
  // buttons if any, and then call CreateButtons, this will call the parent
  // method too.
  RemoveAllChildViews(/*delete_children=*/true);

  CreateButtons();
}

void TouchSelectionMenuChromeOS::CreateButtons() {
  if (action_) {
    views::LabelButton* button = CreateButton(base::UTF8ToUTF16(action_->title),
                                              kSmartTextSelectionActionTag);

    if (action_->bitmap_icon) {
      gfx::ImageSkia original(
          gfx::ImageSkia::CreateFrom1xBitmap(action_->bitmap_icon.value()));
      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          original, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kSmallIconSizeInDip, kSmallIconSizeInDip));
      button->SetImage(views::Button::ButtonState::STATE_NORMAL, icon);
    }

    AddChildView(button);
  }

  views::TouchSelectionMenuViews::CreateButtons();
}

void TouchSelectionMenuChromeOS::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender->tag() != kSmartTextSelectionActionTag) {
    views::TouchSelectionMenuViews::ButtonPressed(sender, event);
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleIntent);
  if (!instance)
    return;

  instance->HandleIntent(std::move(action_->action_intent),
                         std::move(action_->activity));
}

void TouchSelectionMenuChromeOS::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  ash_util::SetupWidgetInitParamsForContainer(
      params, ash::kShellWindowId_SettingBubbleContainer);
}

TouchSelectionMenuChromeOS::~TouchSelectionMenuChromeOS() = default;
