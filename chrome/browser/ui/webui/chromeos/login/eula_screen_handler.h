// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/login/secure_module_util_chromeos.h"
#include "content/public/browser/web_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class CoreOobeView;
class EulaScreen;
class HelpAppLauncher;

// Interface between eula screen and its representation, either WebUI
// or Views one. Note, do not forget to call OnViewDestroyed in the
// dtor.
class EulaView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"eula"};

  virtual ~EulaView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Bind(EulaScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void OnPasswordFetched(const std::string& tpm_password) = 0;
};

// WebUI implementation of EulaScreenView. It is used to interact
// with the eula part of the JS page.
class EulaScreenHandler : public EulaView, public BaseScreenHandler {
 public:
  using TView = EulaView;

  EulaScreenHandler(JSCallsContainer* js_calls_container,
                    CoreOobeView* core_oobe_view);
  ~EulaScreenHandler() override;

  // EulaView implementation:
  void Show() override;
  void Hide() override;
  void Bind(EulaScreen* screen) override;
  void Unbind() override;
  void OnPasswordFetched(const std::string& tpm_password) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

  static void set_eula_url_for_testing(const char* eula_test_url) {
    eula_url_for_testing_ = eula_test_url;
  }

 private:
  // JS messages handlers.
  void HandleOnLearnMore();
  void HandleOnInstallationSettingsPopupOpened();
  void HandleUsageStatsEnabled(bool enabled);

  // Determines the online URL to use.
  std::string GetEulaOnlineUrl();
  static const char* eula_url_for_testing_;

  void UpdateLocalizedValues(::login::SecureModuleUsed secure_module_used);

  EulaScreen* screen_ = nullptr;
  CoreOobeView* core_oobe_view_ = nullptr;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  base::WeakPtrFactory<EulaScreenHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EulaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
