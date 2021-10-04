// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/window_pin_util.h"

#include "ash/wm/window_state.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

void PinWindow(aura::Window* window, bool trusted) {
  DCHECK(window);
  // TODO(crbug.com/1249678): Transition away from property once callers are
  // consolidated.
  window->SetProperty(chromeos::kWindowPinTypeKey,
                      trusted ? chromeos::WindowPinType::kTrustedPinned
                              : chromeos::WindowPinType::kPinned);
}

void UnpinWindow(aura::Window* window) {
  DCHECK(window);
  // TODO(crbug.com/1249678): Transition away from property once callers are
  // consolidated.
  window->SetProperty(chromeos::kWindowPinTypeKey,
                      chromeos::WindowPinType::kNone);
}

chromeos::WindowPinType GetWindowPinType(const aura::Window* window) {
  DCHECK(window);
  return window->GetProperty(chromeos::kWindowPinTypeKey);
}

bool IsWindowPinned(const aura::Window* window) {
  DCHECK(window);
  const ash::WindowState* state = ash::WindowState::Get(window);
  return state->IsPinned();
}
