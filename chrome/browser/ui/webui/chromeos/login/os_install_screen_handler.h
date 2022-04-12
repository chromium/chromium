// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/ash/components/dbus/os_install/os_install_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
class OsInstallScreen;
}

namespace base {
class TimeDelta;
}  // namespace base

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace chromeos {

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
  virtual void SetStatus(OsInstallClient::Status status) = 0;
  virtual void SetServiceLogs(const std::string& service_log) = 0;
  virtual void UpdateCountdownStringWithTime(base::TimeDelta time_left) = 0;
};

class OsInstallScreenHandler : public BaseScreenHandler,
                               public OsInstallScreenView {
 public:
  using TView = OsInstallScreenView;

  OsInstallScreenHandler();
  OsInstallScreenHandler(const OsInstallScreenHandler&) = delete;
  OsInstallScreenHandler& operator=(const OsInstallScreenHandler&) = delete;
  ~OsInstallScreenHandler() override;

 private:
  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;

  // OsInstallScreenView:
  void Show() override;
  void Bind(ash::OsInstallScreen* screen) override;
  void Unbind() override;
  void ShowStep(const char* step) override;
  void SetStatus(OsInstallClient::Status status) override;
  void SetServiceLogs(const std::string& service_log) override;
  void UpdateCountdownStringWithTime(base::TimeDelta time_left) override;

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
