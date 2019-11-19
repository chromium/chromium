// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_keyboard_configuration.h"

#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "ash/ime/ime_controller.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// keyboard_device_configuration interface:

class WaylandKeyboardDeviceConfigurationDelegate
    : public KeyboardDeviceConfigurationDelegate,
      public KeyboardObserver,
      public ash::ImeController::Observer {
 public:
  WaylandKeyboardDeviceConfigurationDelegate(wl_resource* resource,
                                             Keyboard* keyboard)
      : resource_(resource), keyboard_(keyboard) {
    keyboard_->SetDeviceConfigurationDelegate(this);
    keyboard_->AddObserver(this);
    ash::ImeController* ime_controller = ash::Shell::Get()->ime_controller();
    ime_controller->AddObserver(this);
    OnKeyboardLayoutNameChanged(ime_controller->keyboard_layout_name());
  }

  ~WaylandKeyboardDeviceConfigurationDelegate() override {
    ash::Shell::Get()->ime_controller()->RemoveObserver(this);
    if (keyboard_) {
      keyboard_->SetDeviceConfigurationDelegate(nullptr);
      keyboard_->RemoveObserver(this);
    }
  }

  // Overridden from KeyboardObserver:
  void OnKeyboardDestroying(Keyboard* keyboard) override {
    keyboard_ = nullptr;
  }

  // Overridden from KeyboardDeviceConfigurationDelegate:
  void OnKeyboardTypeChanged(bool is_physical) override {
    zcr_keyboard_device_configuration_v1_send_type_change(
        resource_,
        is_physical
            ? ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_KEYBOARD_TYPE_PHYSICAL
            : ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_KEYBOARD_TYPE_VIRTUAL);
  }

  // Overridden from ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override {}

  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override {
    zcr_keyboard_device_configuration_v1_send_layout_change(
        resource_, layout_name.c_str());
  }

 private:
  wl_resource* resource_;
  Keyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardDeviceConfigurationDelegate);
};

void keyboard_device_configuration_destroy(wl_client* client,
                                           wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_keyboard_device_configuration_v1_interface
    keyboard_device_configuration_implementation = {
        keyboard_device_configuration_destroy};

////////////////////////////////////////////////////////////////////////////////
// keyboard_configuration interface:

void keyboard_configuration_get_keyboard_device_configuration(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* keyboard_resource) {
  Keyboard* keyboard = GetUserDataAs<Keyboard>(keyboard_resource);
  if (keyboard->HasDeviceConfigurationDelegate()) {
    wl_resource_post_error(
        resource,
        ZCR_KEYBOARD_CONFIGURATION_V1_ERROR_DEVICE_CONFIGURATION_EXISTS,
        "keyboard has already been associated with a device configuration "
        "object");
    return;
  }

  wl_resource* keyboard_device_configuration_resource = wl_resource_create(
      client, &zcr_keyboard_device_configuration_v1_interface,
      wl_resource_get_version(resource), id);

  SetImplementation(
      keyboard_device_configuration_resource,
      &keyboard_device_configuration_implementation,
      std::make_unique<WaylandKeyboardDeviceConfigurationDelegate>(
          keyboard_device_configuration_resource, keyboard));
}

const struct zcr_keyboard_configuration_v1_interface
    keyboard_configuration_implementation = {
        keyboard_configuration_get_keyboard_device_configuration};

}  // namespace

void bind_keyboard_configuration(wl_client* client,
                                 void* data,
                                 uint32_t version,
                                 uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_keyboard_configuration_v1_interface,
      std::min(version, kZcrKeyboardConfigurationVersion), id);
  wl_resource_set_implementation(
      resource, &keyboard_configuration_implementation, data, nullptr);
}

}  // namespace wayland
}  // namespace exo
