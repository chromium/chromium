// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_keyboard_extension.h"

#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "components/exo/keyboard.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// extended_keyboard interface:

class WaylandExtendedKeyboardImpl : public KeyboardObserver {
 public:
  explicit WaylandExtendedKeyboardImpl(Keyboard* keyboard)
      : keyboard_(keyboard) {
    keyboard_->AddObserver(this);
    keyboard_->SetNeedKeyboardKeyAcks(true);
  }
  WaylandExtendedKeyboardImpl(const WaylandExtendedKeyboardImpl&) = delete;
  WaylandExtendedKeyboardImpl& operator=(const WaylandExtendedKeyboardImpl&) =
      delete;
  ~WaylandExtendedKeyboardImpl() override {
    if (keyboard_) {
      keyboard_->RemoveObserver(this);
      keyboard_->SetNeedKeyboardKeyAcks(false);
    }
  }

  // Overridden from KeyboardObserver:
  void OnKeyboardDestroying(Keyboard* keyboard) override {
    DCHECK(keyboard_ == keyboard);
    keyboard_ = nullptr;
  }

  void AckKeyboardKey(uint32_t serial, bool handled) {
    if (keyboard_)
      keyboard_->AckKeyboardKey(serial, handled);
  }

 private:
  Keyboard* keyboard_;
};

void extended_keyboard_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void extended_keyboard_ack_key(wl_client* client,
                               wl_resource* resource,
                               uint32_t serial,
                               uint32_t handled_state) {
  GetUserDataAs<WaylandExtendedKeyboardImpl>(resource)->AckKeyboardKey(
      serial, handled_state == ZCR_EXTENDED_KEYBOARD_V1_HANDLED_STATE_HANDLED);
}

const struct zcr_extended_keyboard_v1_interface
    extended_keyboard_implementation = {extended_keyboard_destroy,
                                        extended_keyboard_ack_key};

////////////////////////////////////////////////////////////////////////////////
// keyboard_extension interface:

void keyboard_extension_get_extended_keyboard(wl_client* client,
                                              wl_resource* resource,
                                              uint32_t id,
                                              wl_resource* keyboard_resource) {
  Keyboard* keyboard = GetUserDataAs<Keyboard>(keyboard_resource);
  if (keyboard->AreKeyboardKeyAcksNeeded()) {
    wl_resource_post_error(
        resource, ZCR_KEYBOARD_EXTENSION_V1_ERROR_EXTENDED_KEYBOARD_EXISTS,
        "keyboard has already been associated with a extended_keyboard object");
    return;
  }

  wl_resource* extended_keyboard_resource =
      wl_resource_create(client, &zcr_extended_keyboard_v1_interface, 1, id);

  SetImplementation(extended_keyboard_resource,
                    &extended_keyboard_implementation,
                    std::make_unique<WaylandExtendedKeyboardImpl>(keyboard));
}

const struct zcr_keyboard_extension_v1_interface
    keyboard_extension_implementation = {
        keyboard_extension_get_extended_keyboard};

}  // namespace

void bind_keyboard_extension(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_keyboard_extension_v1_interface, version, id);

  wl_resource_set_implementation(resource, &keyboard_extension_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
