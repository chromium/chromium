// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/screen_orientation_delegate_chromeos.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"

namespace {

chromeos::OrientationType ToAshOrientationLockType(
    device::mojom::ScreenOrientationLockType blink_orientation_lock) {
  switch (blink_orientation_lock) {
    case device::mojom::ScreenOrientationLockType::DEFAULT:
    case device::mojom::ScreenOrientationLockType::ANY:
      return chromeos::OrientationType::kAny;
    case device::mojom::ScreenOrientationLockType::PORTRAIT:
      return chromeos::OrientationType::kPortrait;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return chromeos::OrientationType::kPortraitPrimary;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
      return chromeos::OrientationType::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return chromeos::OrientationType::kLandscape;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
      return chromeos::OrientationType::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return chromeos::OrientationType::kLandscapeSecondary;
    case device::mojom::ScreenOrientationLockType::NATURAL:
      return chromeos::OrientationType::kNatural;
  }
  return chromeos::OrientationType::kAny;
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

bool ScreenOrientationDelegateChromeos::ScreenOrientationProviderSupported(
    content::WebContents* web_contents) {
  return display::Screen::GetScreen()->InTabletMode();
}

void ScreenOrientationDelegateChromeos::Unlock(
    content::WebContents* web_contents) {
  ash::Shell::Get()
      ->screen_orientation_controller()
      ->UnlockOrientationForWindow(web_contents->GetNativeView());
}
