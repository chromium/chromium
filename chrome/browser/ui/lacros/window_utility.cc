// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/window_utility.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace lacros_window_utility {

std::string GetRootWindowUniqueId(aura::Window* window) {
  DCHECK(window);

  // On desktop aura there is one WindowTreeHost per top-level window.
  aura::WindowTreeHost* window_tree_host = window->GetRootWindow()->GetHost();
  DCHECK(window_tree_host);

  return window_tree_host->GetUniqueId();
}

}  // namespace lacros_window_utility
