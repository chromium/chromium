// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/window_utility.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window.h"

namespace lacros_window_utility {

std::string GetRootWindowUniqueId(aura::Window* window) {
  DCHECK(window);

  // On desktop aura there is one WindowTreeHost per top-level window.
  aura::WindowTreeHost* window_tree_host = window->GetRootWindow()->GetHost();
  DCHECK(window_tree_host);

  // Lacros is based on Ozone/Wayland, which uses PlatformWindow and
  // aura::WindowTreeHostPlatform.
  aura::WindowTreeHostPlatform* window_tree_host_platform =
      static_cast<aura::WindowTreeHostPlatform*>(window_tree_host);
  return window_tree_host_platform->platform_window()->GetWindowUniqueId();
}

}  // namespace lacros_window_utility
