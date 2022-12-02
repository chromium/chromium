// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/immersive_context_lacros.h"

#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

ImmersiveContextLacros::ImmersiveContextLacros() = default;

ImmersiveContextLacros::~ImmersiveContextLacros() = default;

void ImmersiveContextLacros::OnEnteringOrExitingImmersive(
    chromeos::ImmersiveFullscreenController* controller,
    bool entering) {}

gfx::Rect ImmersiveContextLacros::GetDisplayBoundsInScreen(
    views::Widget* widget) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  return display.bounds();
}

bool ImmersiveContextLacros::DoesAnyWindowHaveCapture() {
  return views::MenuController::GetActiveInstance() != nullptr;
}
