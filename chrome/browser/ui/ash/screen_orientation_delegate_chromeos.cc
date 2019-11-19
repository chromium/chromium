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
    blink::WebScreenOrientationLockType blink_orientation_lock) {
  switch (blink_orientation_lock) {
    case blink::kWebScreenOrientationLockDefault:
    case blink::kWebScreenOrientationLockAny:
      return ash::OrientationLockType::kAny;
    case blink::kWebScreenOrientationLockPortrait:
      return ash::OrientationLockType::kPortrait;
    case blink::kWebScreenOrientationLockPortraitPrimary:
      return ash::OrientationLockType::kPortraitPrimary;
    case blink::kWebScreenOrientationLockPortraitSecondary:
      return ash::OrientationLockType::kPortraitSecondary;
    case blink::kWebScreenOrientationLockLandscape:
      return ash::OrientationLockType::kLandscape;
    case blink::kWebScreenOrientationLockLandscapePrimary:
      return ash::OrientationLockType::kLandscapePrimary;
    case blink::kWebScreenOrientationLockLandscapeSecondary:
      return ash::OrientationLockType::kLandscapeSecondary;
    case blink::kWebScreenOrientationLockNatural:
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
    blink::WebScreenOrientationLockType orientation_lock) {
  ash::Shell::Get()->screen_orientation_controller()->LockOrientationForWindow(
      web_contents->GetNativeView(),
      ToAshOrientationLockType(orientation_lock));
}

bool ScreenOrientationDelegateChromeos::ScreenOrientationProviderSupported() {
  return ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode();
}

void ScreenOrientationDelegateChromeos::Unlock(
    content::WebContents* web_contents) {
  ash::Shell::Get()
      ->screen_orientation_controller()
      ->UnlockOrientationForWindow(web_contents->GetNativeView());
}
