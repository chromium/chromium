// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/borealis/borealis_util.h"

#include "base/strings/string_util.h"
#include "components/exo/shell_surface_util.h"

namespace ash::borealis {

namespace {
// Returns an ID for this window, as set by the Wayland client that created it.
//
// Prefers the value set via xdg_toplevel.set_app_id(), if any. Falls back to
// the value set via zaura_surface.set_startup_id().
// The ID string is owned by the window.
const std::string* WaylandWindowId(const aura::Window* window) {
  const std::string* id = exo::GetShellApplicationId(window);
  if (id) {
    return id;
  }
  return exo::GetShellStartupId(window);
}

}  // namespace

const char kBorealisWindowPrefix[] = "org.chromium.guest_os.borealis.";

const char kBorealisAnonymousPrefix[] = "borealis_anon:";

bool IsBorealisWindowId(const std::string& wayland_window_id) {
  return base::StartsWith(wayland_window_id, kBorealisWindowPrefix);
}

bool IsBorealisWindow(const aura::Window* window) {
  const std::string* id = WaylandWindowId(window);
  if (!id) {
    return false;
  }
  return IsBorealisWindowId(*id);
}

}  // namespace ash::borealis
