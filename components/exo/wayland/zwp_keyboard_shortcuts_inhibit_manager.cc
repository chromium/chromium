// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_keyboard_shortcuts_inhibit_manager.h"

#include <keyboard-shortcuts-inhibit-unstable-v1-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo::wayland {
namespace {

// Tracks the keyboard-shortcut-inhibitor setting, and notifies corresponding
// Surface on unset.
class KeyboardShortcutsInhibitor : public SurfaceObserver {
 public:
  explicit KeyboardShortcutsInhibitor(Surface* surface) : surface_(surface) {
    surface->SetKeyboardShortcutsInhibited(true);
    surface_->AddSurfaceObserver(this);
  }

  ~KeyboardShortcutsInhibitor() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetKeyboardShortcutsInhibited(false);
    }
  }

  void OnSurfaceDestroying(Surface* surface) override {
    DCHECK_EQ(surface, surface_);
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  raw_ptr<Surface> surface_;
};

void keyboard_shortcuts_inhibitor_destroy(wl_client* client,
                                          wl_resource* resource) {
  wl_resource_destroy(resource);
}

constexpr struct zwp_keyboard_shortcuts_inhibitor_v1_interface
    keyboard_shortcuts_inhibitor_implementation = {
        keyboard_shortcuts_inhibitor_destroy,
};

void keyboard_shortcuts_inhibit_manager_destroy(wl_client* client,
                                                wl_resource* resource) {
  wl_resource_destroy(resource);
}

void keyboard_shortcuts_inhibit_manager_inhibit_shortcuts(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* surface_resource,
    wl_resource* seat) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->is_keyboard_shortcuts_inhibited()) {
    wl_resource_post_error(
        resource,
        ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ALREADY_INHIBITED,
        "the associated surface has already been set to inhibit keyboard "
        "shortcuts");
    return;
  }

  uint32_t version = wl_resource_get_version(resource);
  wl_resource* keyboard_shortcuts_inhibitor_resource = wl_resource_create(
      client, &zwp_keyboard_shortcuts_inhibitor_v1_interface, version, id);
  SetImplementation(keyboard_shortcuts_inhibitor_resource,
                    &keyboard_shortcuts_inhibitor_implementation,
                    std::make_unique<KeyboardShortcutsInhibitor>(surface));
}

constexpr struct zwp_keyboard_shortcuts_inhibit_manager_v1_interface
    keyboard_shortcuts_inhibit_manager_implementation = {
        keyboard_shortcuts_inhibit_manager_destroy,
        keyboard_shortcuts_inhibit_manager_inhibit_shortcuts,
};

}  // namespace

void bind_keyboard_shortcuts_inhibit_manager(wl_client* client,
                                             void* data,
                                             uint32_t version,
                                             uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, version,
      id);
  wl_resource_set_implementation(
      resource, &keyboard_shortcuts_inhibit_manager_implementation, data,
      nullptr);
}

}  // namespace exo::wayland
