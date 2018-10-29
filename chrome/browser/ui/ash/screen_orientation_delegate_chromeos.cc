// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/screen_orientation_delegate_chromeos.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"

namespace {

ash::mojom::OrientationLockType ToAshOrientationLockType(
    blink::WebScreenOrientationLockType blink_orientation_lock) {
  switch (blink_orientation_lock) {
    case blink::kWebScreenOrientationLockDefault:
    case blink::kWebScreenOrientationLockAny:
      return ash::mojom::OrientationLockType::kAny;
    case blink::kWebScreenOrientationLockPortrait:
      return ash::mojom::OrientationLockType::kPortrait;
    case blink::kWebScreenOrientationLockPortraitPrimary:
      return ash::mojom::OrientationLockType::kPortraitPrimary;
    case blink::kWebScreenOrientationLockPortraitSecondary:
      return ash::mojom::OrientationLockType::kPortraitSecondary;
    case blink::kWebScreenOrientationLockLandscape:
      return ash::mojom::OrientationLockType::kLandscape;
    case blink::kWebScreenOrientationLockLandscapePrimary:
      return ash::mojom::OrientationLockType::kLandscapePrimary;
    case blink::kWebScreenOrientationLockLandscapeSecondary:
      return ash::mojom::OrientationLockType::kLandscapeSecondary;
    case blink::kWebScreenOrientationLockNatural:
      return ash::mojom::OrientationLockType::kNatural;
  }
  return ash::mojom::OrientationLockType::kAny;
}

}  // namespace

ScreenOrientationDelegateChromeos::ScreenOrientationDelegateChromeos() {
  if (features::IsUsingWindowService()) {
    ash_window_manager_ =
        views::MusClient::Get()
            ->window_tree_client()
            ->BindWindowManagerInterface<ash::mojom::AshWindowManager>();
  }

  content::WebContents::SetScreenOrientationDelegate(this);
}

ScreenOrientationDelegateChromeos::~ScreenOrientationDelegateChromeos() {
  content::WebContents::SetScreenOrientationDelegate(nullptr);
}

bool ScreenOrientationDelegateChromeos::FullScreenRequired(
    content::WebContents* web_contents) {
  return true;
}

void ScreenOrientationDelegateChromeos::Lock(
    content::WebContents* web_contents,
    blink::WebScreenOrientationLockType orientation_lock) {
  if (features::IsUsingWindowService()) {
    ash_window_manager_->LockOrientation(
        aura::WindowMus::Get(web_contents->GetNativeView())->server_id(),
        ToAshOrientationLockType(orientation_lock));
  } else {
    ash::Shell::Get()
        ->screen_orientation_controller()
        ->LockOrientationForWindow(web_contents->GetNativeView(),
                                   ToAshOrientationLockType(orientation_lock));
  }
}

bool ScreenOrientationDelegateChromeos::ScreenOrientationProviderSupported() {
  return TabletModeClient::Get() &&
         TabletModeClient::Get()->tablet_mode_enabled();
}

void ScreenOrientationDelegateChromeos::Unlock(
    content::WebContents* web_contents) {
  if (features::IsUsingWindowService()) {
    ash_window_manager_->UnlockOrientation(
        aura::WindowMus::Get(web_contents->GetNativeView())->server_id());
  } else {
    ash::Shell::Get()
        ->screen_orientation_controller()
        ->UnlockOrientationForWindow(web_contents->GetNativeView());
  }
}
