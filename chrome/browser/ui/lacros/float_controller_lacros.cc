// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/float_controller_lacros.h"

#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/extensions/wayland_extension.h"

FloatControllerLacros::FloatControllerLacros() = default;

FloatControllerLacros::~FloatControllerLacros() = default;

void FloatControllerLacros::ToggleFloat(aura::Window* window) {
  auto* wth_platform =
      static_cast<aura::WindowTreeHostPlatform*>(window->GetHost());
  auto* wayland_extension =
      ui::GetWaylandExtension(*wth_platform->platform_window());
  DCHECK(wayland_extension);

  const bool floated = window->GetProperty(chromeos::kWindowStateTypeKey) ==
                       chromeos::WindowStateType::kFloated;
  wayland_extension->SetFloat(!floated);
}
