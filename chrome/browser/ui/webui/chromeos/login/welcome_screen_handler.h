// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class WelcomeScreen;
}

namespace chromeos {
class CoreOobeView;

// Interface for WelcomeScreenHandler.
class WelcomeView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"connect"};

  virtual ~WelcomeView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds `screen` to the view.
  virtual void Bind(ash::WelcomeScreen* screen) = 0;

  // Unbinds model from the view.
  virtual void Unbind() = 0;

  // Reloads localized contents.
  virtual void ReloadLocalizedContent() = 0;

  // Change the current input method.
  virtual void SetInputMethodId(const std::string& input_method_id) = 0;

  // Shows dialog to confirm starting Demo mode.
  virtual void ShowDemoModeConfirmationDialog() = 0;
  virtual void ShowEditRequisitionDialog(const std::string& requisition) = 0;
  virtual void ShowRemoraRequisitionDialog() = 0;

  // ChromeVox hint.
  virtual void GiveChromeVoxHint() = 0;

  struct A11yState {
    // Whether or not the corresponding feature is turned on.
    const bool high_contrast;
    const bool large_cursor;
    const bool spoken_feedback;
    const bool select_to_speak;
    const bool screen_magnifier;
    const bool docked_magnifier;
    const bool virtual_keyboard;
  };
  // Updates a11y menu state based on the current a11y features state(on/off).
  virtual void UpdateA11yState(const A11yState& state) = 0;

  virtual void SetQuickStartEnabled() = 0;
};

// WebUI implementation of WelcomeScreenView. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class WelcomeScreenHandler : public WelcomeView, public BaseScreenHandler {
 public:
  using TView = WelcomeView;

  explicit WelcomeScreenHandler(CoreOobeView* core_oobe_view);

  WelcomeScreenHandler(const WelcomeScreenHandler&) = delete;
  WelcomeScreenHandler& operator=(const WelcomeScreenHandler&) = delete;

  ~WelcomeScreenHandler() override;

  // WelcomeView:
  void Show() override;
  void Hide() override;
  void Bind(ash::WelcomeScreen* screen) override;
  void Unbind() override;
  void ReloadLocalizedContent() override;
  void SetInputMethodId(const std::string& input_method_id) override;
  void ShowDemoModeConfirmationDialog() override;
  void ShowEditRequisitionDialog(const std::string& requisition) override;
  void ShowRemoraRequisitionDialog() override;
  void GiveChromeVoxHint() override;
  void UpdateA11yState(const A11yState& state) override;
  void SetQuickStartEnabled() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;
  void InitializeDeprecated() override;

 private:
  // JS callbacks.
  void HandleSetLocaleId(const std::string& locale_id);
  void HandleSetInputMethodId(const std::string& input_method_id);
  void HandleSetTimezoneId(const std::string& timezone_id);
  void HandleEnableLargeCursor(bool enabled);
  void HandleEnableHighContrast(bool enabled);
  void HandleEnableVirtualKeyboard(bool enabled);
  void HandleEnableScreenMagnifier(bool enabled);
  void HandleEnableSpokenFeedback(bool /* enabled */);
  void HandleEnableSelectToSpeak(bool /* enabled */);
  void HandleEnableDockedMagnifier(bool /* enabled */);
  void HandleSetDeviceRequisition(const std::string& requisition);
  void HandleRecordChromeVoxHintSpokenSuccess();

  // Returns available timezones.
  static base::Value::List GetTimezoneList();

  CoreOobeView* core_oobe_view_ = nullptr;
  ash::WelcomeScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::WelcomeScreenHandler;
using ::chromeos::WelcomeView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_
