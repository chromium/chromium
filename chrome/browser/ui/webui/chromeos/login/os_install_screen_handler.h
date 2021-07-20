// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/dbus/os_install/os_install_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
class OsInstallScreen;
}

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace chromeos {
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
  virtual void Bind(ash::OsInstallScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  virtual void ShowStep(const char* step) = 0;
  virtual void ShowConfirmStep() = 0;
  virtual void StartInstall() = 0;
};

class OsInstallScreenHandler : public BaseScreenHandler,
                               public OsInstallScreenView,
                               public OsInstallClient::Observer {
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

  // OsInstallScreenView:
  void Show() override;
  void Bind(ash::OsInstallScreen* screen) override;
  void Unbind() override;
  void ShowStep(const char* step) override;
  void ShowConfirmStep() override;
  void StartInstall() override;

  // OsInstallClient::Observer:
  void StatusChanged(OsInstallClient::Status status,
                     const std::string& service_log) override;

  void OsInstallStarted(absl::optional<OsInstallClient::Status> status);

  ash::OsInstallScreen* screen_ = nullptr;

  base::WeakPtrFactory<OsInstallScreenHandler> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::OsInstallScreenHandler;
using ::chromeos::OsInstallScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_
