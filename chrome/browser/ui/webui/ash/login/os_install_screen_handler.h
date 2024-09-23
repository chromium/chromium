// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_

#include <optional>

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chromeos/ash/components/dbus/os_install/os_install_client.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace ash {

class OsInstallScreen;

// Interface for dependency injection between OsInstallScreen and its
// WebUI representation.
class OsInstallScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"os-install",
                                                       "OsInstallScreen"};

  virtual ~OsInstallScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void ShowStep(const char* step) = 0;
  virtual void SetStatus(OsInstallClient::Status status) = 0;
  virtual void SetServiceLogs(const std::string& service_log) = 0;
  virtual void UpdateCountdownStringWithTime(base::TimeDelta time_left) = 0;
  virtual base::WeakPtr<OsInstallScreenView> AsWeakPtr() = 0;
};

class OsInstallScreenHandler final : public BaseScreenHandler,
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

  // OsInstallScreenView:
  void Show() override;
  void ShowStep(const char* step) override;
  void SetStatus(OsInstallClient::Status status) override;
  void SetServiceLogs(const std::string& service_log) override;
  void UpdateCountdownStringWithTime(base::TimeDelta time_left) override;
  base::WeakPtr<OsInstallScreenView> AsWeakPtr() override;

  base::WeakPtrFactory<OsInstallScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OS_INSTALL_SCREEN_HANDLER_H_
