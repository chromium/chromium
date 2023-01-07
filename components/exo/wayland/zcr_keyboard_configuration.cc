// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_keyboard_configuration.h"

#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <linux/input.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// keyboard_device_configuration interface:

class WaylandKeyboardDeviceConfigurationDelegate
    : public KeyboardDeviceConfigurationDelegate,
      public KeyboardObserver,
      public ash::ImeControllerImpl::Observer,
      public ui::InputDeviceEventObserver {
 public:
  WaylandKeyboardDeviceConfigurationDelegate(wl_resource* resource,
                                             Keyboard* keyboard)
      : resource_(resource), keyboard_(keyboard) {
    keyboard_->SetDeviceConfigurationDelegate(this);
    keyboard_->AddObserver(this);
    ash::ImeControllerImpl* ime_controller =
        ash::Shell::Get()->ime_controller();
    ime_controller->AddObserver(this);
    ui::DeviceDataManager::GetInstance()->AddObserver(this);
    ProcessKeyBitsUpdate();
    OnKeyboardLayoutNameChanged(ime_controller->keyboard_layout_name());
  }
  WaylandKeyboardDeviceConfigurationDelegate(
      const WaylandKeyboardDeviceConfigurationDelegate&) = delete;
  WaylandKeyboardDeviceConfigurationDelegate& operator=(
      const WaylandKeyboardDeviceConfigurationDelegate&) = delete;

  ~WaylandKeyboardDeviceConfigurationDelegate() override {
    ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
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
    wl_client_flush(client());
  }

  // Overridden from ImeControllerImpl::Observer:
  void OnCapsLockChanged(bool enabled) override {}

  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override {
    zcr_keyboard_device_configuration_v1_send_layout_change(
        resource_, layout_name.c_str());
    wl_client_flush(client());
  }

  // Overridden from ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override {
    if (!(input_device_types & ui::InputDeviceEventObserver::kKeyboard)) {
      return;
    }
    ProcessKeyBitsUpdate();
  }

 private:
  // Notify key bits update.
  void ProcessKeyBitsUpdate() {
    if (wl_resource_get_version(resource_) <
        ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_SUPPORTED_KEY_BITS_SINCE_VERSION) {
      return;
    }

    // Preparing wayland keybits.
    wl_array wl_key_bits;
    wl_array_init(&wl_key_bits);
    size_t key_bits_len = EVDEV_BITS_TO_INT64(KEY_CNT) * sizeof(uint64_t);
    uint64_t* wl_key_bits_ptr =
        static_cast<uint64_t*>(wl_array_add(&wl_key_bits, key_bits_len));
    if (!wl_key_bits_ptr) {
      wl_array_release(&wl_key_bits);
      return;
    }
    memset(wl_key_bits_ptr, 0, key_bits_len);

    ui::InputController* input_controller =
        ui::OzonePlatform::GetInstance()->GetInputController();
    // Combine supported key bits from all keyboard into single key bits.
    for (const ui::InputDevice& device :
         ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
      const std::vector<uint64_t>& key_bits =
          input_controller->GetKeyboardKeyBits(device.id);
      for (size_t i = 0; i < key_bits.size(); i++) {
        wl_key_bits_ptr[i] |= key_bits[i];
      }
    }

    zcr_keyboard_device_configuration_v1_send_supported_key_bits(resource_,
                                                                 &wl_key_bits);
    wl_array_release(&wl_key_bits);
    wl_client_flush(client());
  }

  wl_client* client() const { return wl_resource_get_client(resource_); }

  wl_resource* resource_;
  Keyboard* keyboard_;
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
      std::min<uint32_t>(version,
                         zcr_keyboard_configuration_v1_interface.version),
      id);
  wl_resource_set_implementation(
      resource, &keyboard_configuration_implementation, data, nullptr);
}

}  // namespace wayland
}  // namespace exo
