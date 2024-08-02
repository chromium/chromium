// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/zcr_keyboard_configuration.h"

#include <linux/input.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xkbcommon/xkbcommon.h>

#include <string_view>

#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#include "ui/events/ozone/layout/xkb/xkb_modifier_converter.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace exo::wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// keyboard_device_configuration interface:

class WaylandKeyboardDeviceConfigurationDelegate
    : public ash::input_method::InputMethodManager::ImeMenuObserver,
      public KeyboardDeviceConfigurationDelegate,
      public KeyboardObserver,
      public ash::ImeController::Observer,
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
    ash::input_method::InputMethodManager::Get()->AddImeMenuObserver(this);
    // Call this once to setup initial installed keyboard layout data.
    ImeMenuListChanged();
    ProcessKeyBitsUpdate();
    OnKeyboardLayoutNameChanged(ime_controller->keyboard_layout_name());
  }
  WaylandKeyboardDeviceConfigurationDelegate(
      const WaylandKeyboardDeviceConfigurationDelegate&) = delete;
  WaylandKeyboardDeviceConfigurationDelegate& operator=(
      const WaylandKeyboardDeviceConfigurationDelegate&) = delete;

  ~WaylandKeyboardDeviceConfigurationDelegate() override {
    ash::input_method::InputMethodManager::Get()->RemoveImeMenuObserver(this);
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

  // Overridden from ImeController::Observer:
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

  // ash::input_method::InputMethodManager::ImeMenuObserver:
  void ImeMenuActivationChanged(bool) override {}

  void ImeMenuListChanged() override {
    // We'll scan ime list changes and notify if a new one was installed.
    // However if we're not sending event to the client, we can return early.
    if (wl_resource_get_version(resource_) <
        ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_LAYOUT_INSTALL_SINCE_VERSION) {
      return;
    }

    ash::input_method::InputMethodManager::State* state =
        ash::input_method::InputMethodManager::Get()->GetActiveIMEState().get();
    if (!state) {
      return;
    }
    std::vector<ash::input_method::InputMethodDescriptor>
        enabled_ime_descriptors = state->GetEnabledInputMethods();

    for (const auto& descriptor : enabled_ime_descriptors) {
      const std::string& keyboard_layout = descriptor.keyboard_layout();
      if (!base::Contains(installed_keyboard_layouts_, keyboard_layout)) {
        sequenced_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(
                &WaylandKeyboardDeviceConfigurationDelegate::GetXkbKeymap,
                keyboard_layout),
            base::BindOnce(&WaylandKeyboardDeviceConfigurationDelegate::
                               OnKeyboardLayoutInstalled,
                           weak_factory_.GetWeakPtr(), keyboard_layout));
      }
    }

    installed_keyboard_layouts_.clear();
    for (const auto& descriptor : enabled_ime_descriptors) {
      const std::string& keyboard_layout = descriptor.keyboard_layout();
      installed_keyboard_layouts_.insert(keyboard_layout);
    }
  }

  void ImeMenuItemsChanged(
      const std::string&,
      const std::vector<ash::input_method::InputMethodManager::MenuItem>&)
      override {}

 private:
  void OnKeyboardLayoutInstalled(
      const std::string& layout_name,
      std::unique_ptr<char, base::FreeDeleter> keymap_str) {
    // Wayland methods should be run in UI Thread.
    DCHECK(base::CurrentUIThread::IsSet());

    std::string_view keymap = keymap_str.get();
    // Send the content of |keymap| with trailing '\0' termination via shared
    // memory.
    base::UnsafeSharedMemoryRegion shared_keymap_region =
        base::UnsafeSharedMemoryRegion::Create(keymap.size() + 1);
    base::WritableSharedMemoryMapping shared_keymap =
        shared_keymap_region.Map();
    base::subtle::PlatformSharedMemoryRegion platform_shared_keymap =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(shared_keymap_region));
    DCHECK(shared_keymap.IsValid());

    std::memcpy(shared_keymap.memory(), keymap.data(), keymap.size());
    static_cast<uint8_t*>(shared_keymap.memory())[keymap.size()] = '\0';

    zcr_keyboard_device_configuration_v1_send_layout_install(
        resource_, layout_name.c_str(), WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
        platform_shared_keymap.GetPlatformHandle().fd, keymap.size() + 1);
    wl_client_flush(client());
  }

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

  // This method shouldn't run on UI thread since it involves I/O operation.
  static std::unique_ptr<char, base::FreeDeleter> GetXkbKeymap(
      const std::string& layout_name) {
    std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context{
        xkb_context_new(XKB_CONTEXT_NO_FLAGS)};
    std::string layout_id, layout_variant;
    ui::XkbKeyboardLayoutEngine::ParseLayoutName(layout_name, &layout_id,
                                                 &layout_variant);
    xkb_rule_names names = {.rules = nullptr,
                            .model = "pc101",
                            .layout = layout_id.c_str(),
                            .variant = layout_variant.c_str(),
                            .options = ""};

    std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap(
        xkb_keymap_new_from_names(xkb_context.get(), &names,
                                  XKB_KEYMAP_COMPILE_NO_FLAGS));

    return std::unique_ptr<char, base::FreeDeleter>(
        xkb_keymap_get_as_string(xkb_keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
  }

  wl_client* client() const { return wl_resource_get_client(resource_); }

  raw_ptr<wl_resource> resource_;
  raw_ptr<Keyboard> keyboard_;

  // List of acknowledged installed keyboard layouts. Used to determine if there
  // are new keyboard layouts installed.
  std::set<std::string> installed_keyboard_layouts_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})};
  base::WeakPtrFactory<WaylandKeyboardDeviceConfigurationDelegate>
      weak_factory_{this};
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

}  // namespace exo::wayland
