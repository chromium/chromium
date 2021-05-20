// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace chromeos {

class OsInstallScreen;
class JSCallsContainer;

// Interface for dependency injection between OsInstallScreen and its
// WebUI representation.
class OsInstallScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"os-install"};

  virtual ~OsInstallScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Binds |screen| to the view.
  virtual void Bind(OsInstallScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
};

class OsInstallScreenHandler : public BaseScreenHandler,
                               public OsInstallScreenView {
 public:
  using TView = OsInstallScreenView;

  explicit OsInstallScreenHandler(JSCallsContainer* js_calls_container);
  OsInstallScreenHandler(const OsInstallScreenHandler&) = delete;
  OsInstallScreenHandler& operator=(const OsInstallScreenHandler&) = delete;
  ~OsInstallScreenHandler() override;

 private:
  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

  // OsInstallScreenView:
  void Show() override;
  void Bind(OsInstallScreen* screen) override;
  void Unbind() override;

  OsInstallScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_
