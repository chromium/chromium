// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/immersive_context_lacros.h"

#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "ui/views/widget/widget.h"

ImmersiveContextLacros::ImmersiveContextLacros() = default;

ImmersiveContextLacros::~ImmersiveContextLacros() = default;

void ImmersiveContextLacros::OnEnteringOrExitingImmersive(
    chromeos::ImmersiveFullscreenController* controller,
    bool entering) {
  aura::Window* window = controller->widget()->GetNativeWindow();

  // Lacros is based on Ozone/Wayland, which uses ui::PlatformWindow and
  // views::DesktopWindowTreeHostLacros.
  auto* wayland_extension =
      views::DesktopWindowTreeHostLacros::From(window->GetHost())
          ->GetWaylandExtension();

  wayland_extension->SetImmersiveFullscreenStatus(entering);
}

gfx::Rect ImmersiveContextLacros::GetDisplayBoundsInScreen(
    views::Widget* widget) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  return display.bounds();
}

bool ImmersiveContextLacros::DoesAnyWindowHaveCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}
