// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface between HID detection screen and its representation, either WebUI
// or Views one.
class HIDDetectionView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"hid-detection",
                                                       "HIDDetectionScreen"};

  virtual ~HIDDetectionView() = default;

  virtual void Show() = 0;
  virtual void SetKeyboardState(const std::string& value) = 0;
  virtual void SetMouseState(const std::string& value) = 0;
  virtual void SetTouchscreenDetectedState(bool value) = 0;
  virtual void SetKeyboardPinCode(const std::string& value) = 0;
  virtual void SetPinDialogVisible(bool value) = 0;
  virtual void SetNumKeysEnteredPinCode(int value) = 0;
  virtual void SetPointingDeviceName(const std::string& value) = 0;
  virtual void SetKeyboardDeviceName(const std::string& value) = 0;
  virtual void SetContinueButtonEnabled(bool value) = 0;
  virtual base::WeakPtr<HIDDetectionView> AsWeakPtr() = 0;
};

// WebUI implementation of HIDDetectionScreenView.
class HIDDetectionScreenHandler final : public HIDDetectionView,
                                        public BaseScreenHandler {
 public:
  using TView = HIDDetectionView;

  HIDDetectionScreenHandler();

  HIDDetectionScreenHandler(const HIDDetectionScreenHandler&) = delete;
  HIDDetectionScreenHandler& operator=(const HIDDetectionScreenHandler&) =
      delete;

  ~HIDDetectionScreenHandler() override;

  // HIDDetectionView implementation:
  void Show() override;
  void SetKeyboardState(const std::string& value) override;
  void SetMouseState(const std::string& value) override;
  void SetTouchscreenDetectedState(bool value) override;
  void SetKeyboardPinCode(const std::string& value) override;
  void SetPinDialogVisible(bool value) override;
  void SetNumKeysEnteredPinCode(int value) override;
  void SetPointingDeviceName(const std::string& value) override;
  void SetKeyboardDeviceName(const std::string& value) override;
  void SetContinueButtonEnabled(bool value) override;
  base::WeakPtr<HIDDetectionView> AsWeakPtr() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  void GetAdditionalParameters(base::Value::Dict* dict) override;

  // State that has been exported to JS. Used by tests.
  std::string keyboard_state_for_test() const { return keyboard_state_; }
  std::string mouse_state_for_test() const { return mouse_state_; }
  std::string keyboard_pin_code_for_test() const { return keyboard_pin_code_; }
  bool num_keys_entered_expected_for_test() const {
    return num_keys_entered_expected_;
  }
  int num_keys_entered_pin_code_for_test() const {
    return num_keys_entered_pin_code_;
  }
  std::string mouse_device_name_for_test() const { return mouse_device_name_; }
  std::string keyboard_device_name_for_test() const {
    return keyboard_device_name_;
  }
  std::string keyboard_device_label_for_test() const {
    return keyboard_device_label_;
  }
  bool continue_button_enabled_for_test() const {
    return continue_button_enabled_;
  }

 private:
  // Cached values that have been sent to JS. Used by tests.
  std::string keyboard_state_;
  std::string mouse_state_;
  std::string keyboard_pin_code_;
  bool num_keys_entered_expected_ = false;
  int num_keys_entered_pin_code_ = 0;
  std::string mouse_device_name_;
  std::string keyboard_device_name_;
  std::string keyboard_device_label_;
  bool continue_button_enabled_ = false;
  base::WeakPtrFactory<HIDDetectionView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_
