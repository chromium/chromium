// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

HIDDetectionScreenHandler::HIDDetectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

HIDDetectionScreenHandler::~HIDDetectionScreenHandler() = default;

void HIDDetectionScreenHandler::Show() {
  ShowInWebUI();
}

void HIDDetectionScreenHandler::SetKeyboardState(const std::string& value) {
  keyboard_state_ = value;
  CallExternalAPI("setKeyboardState", value);
}

void HIDDetectionScreenHandler::SetMouseState(const std::string& value) {
  mouse_state_ = value;
  CallExternalAPI("setMouseState", value);
}

void HIDDetectionScreenHandler::SetTouchscreenDetectedState(bool value) {
  CallExternalAPI("setTouchscreenDetectedState", value);
}

void HIDDetectionScreenHandler::SetKeyboardPinCode(const std::string& value) {
  keyboard_pin_code_ = value;
  CallExternalAPI("setKeyboardPinCode", value);
}

void HIDDetectionScreenHandler::SetPinDialogVisible(bool value) {
  num_keys_entered_expected_ = value;
  CallExternalAPI("setPinDialogVisible", value);
}

void HIDDetectionScreenHandler::SetNumKeysEnteredPinCode(int value) {
  num_keys_entered_pin_code_ = value;
  CallExternalAPI("setNumKeysEnteredPinCode", value);
}

void HIDDetectionScreenHandler::SetPointingDeviceName(
    const std::string& value) {
  mouse_device_name_ = value;
  CallExternalAPI("setPointingDeviceName", value);
}

void HIDDetectionScreenHandler::SetKeyboardDeviceName(
    const std::string& value) {
  keyboard_device_name_ = value;
  CallExternalAPI("setKeyboardDeviceName", value);
}

void HIDDetectionScreenHandler::SetContinueButtonEnabled(bool value) {
  continue_button_enabled_ = value;
  CallExternalAPI("setContinueButtonEnabled", value);
}

base::WeakPtr<HIDDetectionView> HIDDetectionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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
  builder->Add("bluetoothPairingEnterKeys", IDS_BLUETOOTH_PAIRING_ENTER_KEYS);
  builder->Add("bluetoothEnterKey", IDS_BLUETOOTH_PAIRING_ENTER_KEY);
  builder->Add("bluetoothPairNewDevice",
               IDS_BLUETOOTH_PAIRING_PAIR_NEW_DEVICES);
  builder->Add("bluetoothPair", IDS_BLUETOOTH_PAIRING_PAIR);
  builder->Add("hidDetectionA11yContinueEnabled",
               IDS_HID_DETECTION_A11Y_CONTINUE_BUTTON_ENABLED);
  builder->Add("hidDetectionA11yContinueDisabled",
               IDS_HID_DETECTION_A11Y_CONTINUE_BUTTON_DISABLED);
}

void HIDDetectionScreenHandler::GetAdditionalParameters(
    base::Value::Dict* dict) {
  BaseScreenHandler::GetAdditionalParameters(dict);
}

}  // namespace ash
