// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/values.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/login/secure_module_util_chromeos.h"

namespace ash {
class EulaScreen;
}

namespace chromeos {

// Interface between eula screen and its representation, either WebUI
// or Views one. Note, do not forget to call OnViewDestroyed in the
// dtor.
class EulaView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"oobe-eula-md"};

  virtual ~EulaView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Bind(ash::EulaScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void ShowStatsUsageLearnMore() = 0;
  virtual void ShowAdditionalTosDialog() = 0;
  virtual void ShowSecuritySettingsDialog() = 0;
  virtual void HideSecuritySettingsInfo() = 0;
  virtual void HideBackButton() = 0;
};

// WebUI implementation of EulaScreenView. It is used to interact
// with the eula part of the JS page.
class EulaScreenHandler : public EulaView, public BaseScreenHandler {
 public:
  using TView = EulaView;

  EulaScreenHandler();

  EulaScreenHandler(const EulaScreenHandler&) = delete;
  EulaScreenHandler& operator=(const EulaScreenHandler&) = delete;

  ~EulaScreenHandler() override;

  // EulaView implementation:
  void Show() override;
  void Hide() override;
  void Bind(ash::EulaScreen* screen) override;
  void Unbind() override;
  void ShowStatsUsageLearnMore() override;
  void ShowAdditionalTosDialog() override;
  void ShowSecuritySettingsDialog() override;
  void HideSecuritySettingsInfo() override;
  void HideBackButton() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;
  void InitializeDeprecated() override;

 private:
  // Determines the online URL to use.
  std::string GetEulaOnlineUrl();
  std::string GetAdditionalToSUrl();

  void UpdateTpmDesc(::login::SecureModuleUsed secure_module_used);

  ash::EulaScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Booleans to control parts of UI for different flows.
  bool security_settings_hidden_ = false;
  bool back_button_hidden_ = false;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  base::WeakPtrFactory<EulaScreenHandler> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::EulaScreenHandler;
using ::chromeos::EulaView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
