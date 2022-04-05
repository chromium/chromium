// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class DemoPreferencesScreen;
}

namespace chromeos {

// Interface of the demo mode preferences screen view.
class DemoPreferencesScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"demo-preferences"};

  virtual ~DemoPreferencesScreenView();

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets view and screen.
  virtual void Bind(ash::DemoPreferencesScreen* screen) = 0;

  // Called to set the input method id on JS side.
  virtual void SetInputMethodId(const std::string& input_method) = 0;
};

// WebUI implementation of DemoPreferencesScreenView.
class DemoPreferencesScreenHandler : public BaseScreenHandler,
                                     public DemoPreferencesScreenView {
 public:
  using TView = DemoPreferencesScreenView;

  DemoPreferencesScreenHandler();

  DemoPreferencesScreenHandler(const DemoPreferencesScreenHandler&) = delete;
  DemoPreferencesScreenHandler& operator=(const DemoPreferencesScreenHandler&) =
      delete;

  ~DemoPreferencesScreenHandler() override;

  // DemoPreferencesScreenView:
  void Show() override;
  void Hide() override;
  void Bind(ash::DemoPreferencesScreen* screen) override;
  void SetInputMethodId(const std::string& input_method) override;

  // BaseScreenHandler:
  void InitializeDeprecated() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleSetDemoModeCountry(const std::string& country_id);

  ash::DemoPreferencesScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::DemoPreferencesScreenHandler;
using ::chromeos::DemoPreferencesScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_
