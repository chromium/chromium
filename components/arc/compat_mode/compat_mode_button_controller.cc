// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/compat_mode_button_controller.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "components/arc/compat_mode/arc_window_property_util.h"
#include "components/arc/compat_mode/resize_util.h"
#include "components/arc/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

namespace arc {

CompatModeButtonController::CompatModeButtonController() = default;
CompatModeButtonController::~CompatModeButtonController() = default;

void CompatModeButtonController::Update(
    ArcResizeLockPrefDelegate* pref_delegate,
    aura::Window* window) {
  DCHECK(ash::IsArcWindow(window));

  const auto app_id = GetAppId(window);
  if (!app_id)
    return;
  auto* const frame_header = GetFrameHeader(window);
  const auto resize_lock_state = pref_delegate->GetResizeLockState(*app_id);
  if (resize_lock_state == mojom::ArcResizeLockState::UNDEFINED ||
      resize_lock_state == mojom::ArcResizeLockState::READY) {
    frame_header->SetCenterButton(nullptr);
    return;
  }
  auto* compat_mode_button = frame_header->GetCenterButton();
  if (!compat_mode_button) {
    // The ownership is transferred implicitly with AddChildView in HeaderView,
    // but ideally we want to explicitly manage the lifecycle of this resource.
    compat_mode_button = new chromeos::FrameCenterButton(
        base::BindRepeating(&CompatModeButtonController::ToggleResizeToggleMenu,
                            GetWeakPtr(), window, pref_delegate));
    compat_mode_button->SetSubImage(views::kMenuDropArrowIcon);
    frame_header->SetCenterButton(compat_mode_button);

    auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
    // Ideally, we want HeaderView to update properties, but as currently
    // the center button is set to FrameHeader, we need to call this explicitly.
    // |frame_view| can be null in unittest.
    if (frame_view)
      frame_view->GetHeaderView()->UpdateCaptionButtons();
  }

  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);

  switch (PredictCurrentMode(window)) {
    case ResizeCompatMode::kPhone:
      compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                                   views::FrameCaptionButton::Animate::kNo,
                                   ash::kSystemMenuPhoneIcon);
      compat_mode_button->SetText(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_PHONE));
      compat_mode_button->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_PHONE));
      if (resize_lock_type == ash::ArcResizeLockType::FULLY_LOCKED) {
        compat_mode_button->SetTooltipText(l10n_util::GetStringUTF16(
            IDS_ASH_ARC_APP_COMPAT_DISABLED_COMPAT_MODE_BUTTON_TOOLTIP_PHONE));
      }
      break;
    case ResizeCompatMode::kTablet:
      compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                                   views::FrameCaptionButton::Animate::kNo,
                                   ash::kSystemMenuTabletIcon);
      compat_mode_button->SetText(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_TABLET));
      compat_mode_button->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_TABLET));
      break;
    case ResizeCompatMode::kResizable:
      compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                                   views::FrameCaptionButton::Animate::kNo,
                                   kResizableIcon);
      compat_mode_button->SetText(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE));
      compat_mode_button->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE));
      break;
  }

  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_LIMITED:
    case ash::ArcResizeLockType::RESIZABLE:
      compat_mode_button->SetEnabled(true);
      break;
    case ash::ArcResizeLockType::FULLY_LOCKED:
      compat_mode_button->SetEnabled(false);
      break;
  }

  UpdateAshAccelerator(pref_delegate, window);
}

base::WeakPtr<CompatModeButtonController>
CompatModeButtonController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

chromeos::FrameHeader* CompatModeButtonController::GetFrameHeader(
    aura::Window* window) {
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
  return frame_view->GetHeaderView()->GetFrameHeader();
}

void CompatModeButtonController::UpdateAshAccelerator(
    ArcResizeLockPrefDelegate* pref_delegate,
    aura::Window* window) {
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
  // |frame_view| can be null in unittest.
  if (!frame_view)
    return;

  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);
  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_LIMITED:
    case ash::ArcResizeLockType::RESIZABLE:
      frame_view->SetToggleResizeLockMenuCallback(base::BindRepeating(
          &CompatModeButtonController::ToggleResizeToggleMenu, GetWeakPtr(),
          window, pref_delegate));
      break;
    case ash::ArcResizeLockType::FULLY_LOCKED:
      frame_view->ClearToggleResizeLockMenuCallback();
      break;
  }
}

void CompatModeButtonController::ToggleResizeToggleMenu(
    aura::Window* window,
    ArcResizeLockPrefDelegate* pref_delegate) {
  if (!window || !ash::IsArcWindow(window))
    return;

  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);
  DCHECK(frame_view);
  const auto* compat_mode_button =
      frame_view->GetHeaderView()->GetFrameHeader()->GetCenterButton();
  if (!compat_mode_button || !compat_mode_button->GetEnabled())
    return;
  resize_toggle_menu_.reset();
  resize_toggle_menu_ =
      std::make_unique<ResizeToggleMenu>(frame_view->frame(), pref_delegate);
}

}  // namespace arc
