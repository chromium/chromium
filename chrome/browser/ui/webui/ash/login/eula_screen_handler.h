// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_EULA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_EULA_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/secure_module_util_chromeos.h"

namespace ash {

class EulaScreen;
class HelpAppLauncher;

// Interface between eula screen and its representation, either WebUI
// or Views one.
class EulaView : public base::SupportsWeakPtr<EulaView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"oobe-eula-md",
                                                       "EulaScreen"};

  virtual ~EulaView() = default;

  virtual void Show(const bool is_cloud_ready_update_flow) = 0;
  virtual void Hide() = 0;
  virtual void SetUsageStatsEnabled(bool) = 0;
  virtual void ShowStatsUsageLearnMore() = 0;
  virtual void ShowAdditionalTosDialog() = 0;
  virtual void ShowSecuritySettingsDialog() = 0;
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
  void Show(const bool is_cloud_ready_update_flow) override;
  void Hide() override;
  void SetUsageStatsEnabled(bool) override;
  void ShowStatsUsageLearnMore() override;
  void ShowAdditionalTosDialog() override;
  void ShowSecuritySettingsDialog() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

 private:
  // Determines the online URL to use.
  std::string GetEulaOnlineUrl();
  std::string GetAdditionalToSUrl();

  void UpdateTpmDesc(::login::SecureModuleUsed secure_module_used);

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  base::WeakPtrFactory<EulaScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_EULA_SCREEN_HANDLER_H_
