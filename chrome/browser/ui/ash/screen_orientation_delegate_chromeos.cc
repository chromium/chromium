// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/screen_orientation_delegate_chromeos.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "content/public/browser/web_contents.h"

namespace {

ash::OrientationLockType ToAshOrientationLockType(
    device::mojom::ScreenOrientationLockType blink_orientation_lock) {
  switch (blink_orientation_lock) {
    case device::mojom::ScreenOrientationLockType::DEFAULT:
    case device::mojom::ScreenOrientationLockType::ANY:
      return ash::OrientationLockType::kAny;
    case device::mojom::ScreenOrientationLockType::PORTRAIT:
      return ash::OrientationLockType::kPortrait;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return ash::OrientationLockType::kPortraitPrimary;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
      return ash::OrientationLockType::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return ash::OrientationLockType::kLandscape;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
      return ash::OrientationLockType::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return ash::OrientationLockType::kLandscapeSecondary;
    case device::mojom::ScreenOrientationLockType::NATURAL:
      return ash::OrientationLockType::kNatural;
  }
  return ash::OrientationLockType::kAny;
}

}  // namespace

ScreenOrientationDelegateChromeos::ScreenOrientationDelegateChromeos() {
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
    device::mojom::ScreenOrientationLockType orientation_lock) {
  ash::Shell::Get()->screen_orientation_controller()->LockOrientationForWindow(
      web_contents->GetNativeView(),
      ToAshOrientationLockType(orientation_lock));
}

bool ScreenOrientationDelegateChromeos::ScreenOrientationProviderSupported() {
  return ash::TabletMode::IsInTabletMode();
}

void ScreenOrientationDelegateChromeos::Unlock(
    content::WebContents* web_contents) {
  ash::Shell::Get()
      ->screen_orientation_controller()
      ->UnlockOrientationForWindow(web_contents->GetNativeView());
}
