// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/snap_controller_lacros.h"

#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

namespace {

ui::WaylandWindowSnapDirection ToWaylandWindowSnapDirection(
    chromeos::SnapDirection snap) {
  switch (snap) {
    case chromeos::SnapDirection::kNone:
      return ui::WaylandWindowSnapDirection::kNone;
    case chromeos::SnapDirection::kPrimary:
      return ui::WaylandWindowSnapDirection::kPrimary;
    case chromeos::SnapDirection::kSecondary:
      return ui::WaylandWindowSnapDirection::kSecondary;
  }
}

ui::WaylandToplevelExtension* WaylandToplevelExtensionForAuraWindow(
    aura::Window* window) {
  // Lacros is based on Ozone/Wayland, which uses ui::PlatformWindow and
  // views::DesktopWindowTreeHostLacros.
  if (auto* host = views::DesktopWindowTreeHostLacros::From(window->GetHost()))
    return host->GetWaylandToplevelExtension();

  return nullptr;
}

}  // namespace

SnapControllerLacros::SnapControllerLacros() = default;
SnapControllerLacros::~SnapControllerLacros() = default;

bool SnapControllerLacros::CanSnap(aura::Window* window) {
  // TODO(crbug.com/40154369): Implement this method similarly to
  // ash::WindowState::CanSnap().
  return true;
}
void SnapControllerLacros::ShowSnapPreview(aura::Window* window,
                                           chromeos::SnapDirection snap,
                                           bool allow_haptic_feedback) {
  if (auto* wayland_extension = WaylandToplevelExtensionForAuraWindow(window)) {
    wayland_extension->ShowSnapPreview(ToWaylandWindowSnapDirection(snap),
                                       allow_haptic_feedback);
  }
}

void SnapControllerLacros::CommitSnap(aura::Window* window,
                                      chromeos::SnapDirection snap,
                                      float snap_ratio,
                                      SnapRequestSource snap_request_source) {
  if (auto* wayland_extension = WaylandToplevelExtensionForAuraWindow(window)) {
    wayland_extension->CommitSnap(ToWaylandWindowSnapDirection(snap),
                                  snap_ratio);
  }
}
