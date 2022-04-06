// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace chromeos {

constexpr StaticOobeScreenId HIDDetectionView::kScreenId;

HIDDetectionScreenHandler::HIDDetectionScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated("login.HIDDetectionScreen.userActed");
}

HIDDetectionScreenHandler::~HIDDetectionScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void HIDDetectionScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }

  ShowInWebUI();
}

void HIDDetectionScreenHandler::Hide() {
}

void HIDDetectionScreenHandler::Bind(HIDDetectionScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
  if (IsJavascriptAllowed())
    InitializeDeprecated();
}

void HIDDetectionScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

void HIDDetectionScreenHandler::SetKeyboardState(const std::string& value) {
  keyboard_state_ = value;
  CallJS("login.HIDDetectionScreen.setKeyboardState", value);
}

void HIDDetectionScreenHandler::SetMouseState(const std::string& value) {
  mouse_state_ = value;
  CallJS("login.HIDDetectionScreen.setMouseState", value);
}

void HIDDetectionScreenHandler::SetTouchscreenDetectedState(bool value) {
  CallJS("login.HIDDetectionScreen.setTouchscreenDetectedState", value);
}

void HIDDetectionScreenHandler::SetKeyboardPinCode(const std::string& value) {
  keyboard_pin_code_ = value;
  CallJS("login.HIDDetectionScreen.setKeyboardPinCode", value);
}

void HIDDetectionScreenHandler::SetPinDialogVisible(bool value) {
  num_keys_entered_expected_ = value;
  CallJS("login.HIDDetectionScreen.setPinDialogVisible", value);
}

void HIDDetectionScreenHandler::SetNumKeysEnteredPinCode(int value) {
  num_keys_entered_pin_code_ = value;
  CallJS("login.HIDDetectionScreen.setNumKeysEnteredPinCode", value);
}

void HIDDetectionScreenHandler::SetPointingDeviceName(
    const std::string& value) {
  mouse_device_name_ = value;
  CallJS("login.HIDDetectionScreen.setPointingDeviceName", value);
}

void HIDDetectionScreenHandler::SetKeyboardDeviceName(
    const std::string& value) {
  keyboard_device_name_ = value;
  CallJS("login.HIDDetectionScreen.setKeyboardDeviceName", value);
}

void HIDDetectionScreenHandler::SetContinueButtonEnabled(bool value) {
  continue_button_enabled_ = value;
  CallJS("login.HIDDetectionScreen.setContinueButtonEnabled", value);
}

void HIDDetectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("hidDetectionContinue", IDS_HID_DETECTION_CONTINUE_BUTTON);
  builder->Add("hidDetectionInvitation", IDS_HID_DETECTION_INVITATION_TEXT);
  builder->Add("hidDetectionPrerequisites",
      IDS_HID_DETECTION_PRECONDITION_TEXT);
  builder->Add("hidDetectionPrerequisitesTouchscreen",
               IDS_HID_DETECTION_PRECONDITION_TOUCHSCREEN_TEXT);
  builder->Add("hidDetectionMouseSearching", IDS_HID_DETECTION_SEARCHING_MOUSE);
  builder->Add("hidDetectionKeyboardSearching",
      IDS_HID_DETECTION_SEARCHING_KEYBOARD);
  builder->Add("hidDetectionUSBMouseConnected",
      IDS_HID_DETECTION_CONNECTED_USB_MOUSE);
  builder->Add("hidDetectionPointingDeviceConnected",
      IDS_HID_DETECTION_CONNECTED_POINTING_DEVICE);
  builder->Add("hidDetectionKeyboardPairing",
               IDS_HID_DETECTION_PAIRING_BLUETOOTH_KEYBOARD);
  builder->Add("hidDetectionUSBKeyboardConnected",
      IDS_HID_DETECTION_CONNECTED_USB_KEYBOARD);
  builder->Add("hidDetectionBTMousePaired",
      IDS_HID_DETECTION_PAIRED_BLUETOOTH_MOUSE);
  builder->Add("hidDetectionBTEnterKey", IDS_HID_DETECTION_BLUETOOTH_ENTER_KEY);
  builder->Add("hidDetectionPinDialogTitle",
               IDS_HID_DETECTION_PAIRING_BLUETOOTH_KEYBOARD_PIN_DIALOG_TITLE);
  builder->Add("hidDetectionBluetoothPairingCode",
               IDS_HID_DETECTION_BLUETOOTH_PAIRING_CODE);
  builder->Add("hidDetectionBluetoothPairingCodeExplanation",
               IDS_HID_DETECTION_BLUETOOTH_PAIRING_CODE_EXPLANATION);
  builder->Add("hidDetectionBluetoothKeyboardPaired",
               IDS_HID_DETECTION_PAIRED_BLUETOOTH_KEYBOARD);
  builder->Add("oobeModalDialogClose", IDS_CHROMEOS_OOBE_CLOSE_DIALOG);
  builder->Add("hidDetectionTouchscreenDetected",
               IDS_HID_DETECTION_DETECTED_TOUCHSCREEN);
}

void HIDDetectionScreenHandler::DeclareJSCallbacks() {
  AddCallback(
      "HIDDetectionScreen.emulateDevicesConnectedForTesting",
      &HIDDetectionScreenHandler::HandleEmulateDevicesConnectedForTesting);
}

void HIDDetectionScreenHandler::HandleEmulateDevicesConnectedForTesting() {
  auto touchscreen = device::mojom::InputDeviceInfo::New();
  touchscreen->id = "fake_touchscreen";
  touchscreen->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  touchscreen->type = device::mojom::InputDeviceType::TYPE_UNKNOWN;
  touchscreen->is_touchscreen = true;
  screen_->InputDeviceAddedForTesting(std::move(touchscreen));  // IN-TEST

  auto mouse = device::mojom::InputDeviceInfo::New();
  mouse->id = "fake_mouse";
  mouse->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  mouse->type = device::mojom::InputDeviceType::TYPE_USB;
  mouse->is_mouse = true;
  screen_->InputDeviceAddedForTesting(std::move(mouse));  // IN-TEST

  auto keyboard = device::mojom::InputDeviceInfo::New();
  keyboard->id = "fake_keyboard";
  keyboard->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  keyboard->type = device::mojom::InputDeviceType::TYPE_USB;
  keyboard->is_keyboard = true;
  screen_->InputDeviceAddedForTesting(std::move(keyboard));  // IN-TEST
}

void HIDDetectionScreenHandler::InitializeDeprecated() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
