// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/window_pin_util.h"

#include "chrome/browser/ui/lacros/window_properties.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "ui/aura/window.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

void PinWindow(aura::Window* window, bool trusted) {
  CHECK(window);

  auto* pinned_mode_extension =
      views::DesktopWindowTreeHostLacros::From(window->GetHost())
          ->GetPinnedModeExtension();

  // kWindowPinTypeKey should be updated when Ash side acknowledges the state
  // change, but this requires pinned state to be supported on configure event.
  // If it's not yet supported, set it synchronously here.
  if (!pinned_mode_extension->SupportsConfigurePinnedState()) {
    window->SetProperty(lacros::kWindowPinTypeKey,
                        trusted ? chromeos::WindowPinType::kTrustedPinned
                                : chromeos::WindowPinType::kPinned);
  }

  pinned_mode_extension->Pin(trusted);
}

void UnpinWindow(aura::Window* window) {
  CHECK(window);

  auto* pinned_mode_extension =
      views::DesktopWindowTreeHostLacros::From(window->GetHost())
          ->GetPinnedModeExtension();

  // kWindowPinTypeKey should be updated when Ash side acknowledges the state
  // change, but this requires pinned state to be supported on configure event.
  // If it's not yet supported, set it synchronously here.
  if (!pinned_mode_extension->SupportsConfigurePinnedState()) {
    window->SetProperty(lacros::kWindowPinTypeKey,
                        chromeos::WindowPinType::kNone);
  }

  pinned_mode_extension->Unpin();
}

chromeos::WindowPinType GetWindowPinType(const aura::Window* window) {
  CHECK(window);
  return window->GetProperty(lacros::kWindowPinTypeKey);
}

bool IsWindowPinned(const aura::Window* window) {
  CHECK(window);
  return GetWindowPinType(window) != chromeos::WindowPinType::kNone;
}
