// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// Interface of the demo mode preferences screen view.
class DemoPreferencesScreenView
    : public base::SupportsWeakPtr<DemoPreferencesScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"demo-preferences",
                                                       "DemoPreferencesScreen"};

  virtual ~DemoPreferencesScreenView();

  // Shows the contents of the screen.
  virtual void Show() = 0;

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
  void SetInputMethodId(const std::string& input_method) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::DemoPreferencesScreenHandler;
using ::chromeos::DemoPreferencesScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_
