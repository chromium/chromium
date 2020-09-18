// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "rlz/buildflags/buildflags.h"
#include "url/gurl.h"

namespace chromeos {

constexpr StaticOobeScreenId EulaView::kScreenId;

EulaScreenHandler::EulaScreenHandler(JSCallsContainer* js_calls_container,
                                     CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId, js_calls_container),
      core_oobe_view_(core_oobe_view) {
  set_user_acted_method_path("login.EulaScreen.userActed");
}

EulaScreenHandler::~EulaScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void EulaScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void EulaScreenHandler::Hide() {
}

void EulaScreenHandler::Bind(EulaScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
  if (page_is_ready())
    Initialize();
}

void EulaScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

std::string EulaScreenHandler::GetEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kOnlineEulaURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

std::string EulaScreenHandler::GetAdditionalToSUrl() {
  return base::StringPrintf(chrome::kAdditionalToSOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

void EulaScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("eulaScreenAccessibleTitle", IDS_EULA_SCREEN_ACCESSIBLE_TITLE);
  builder->Add("checkboxLogging", IDS_EULA_CHECKBOX_ENABLE_LOGGING);
  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("next", IDS_EULA_NEXT_BUTTON);
  builder->Add("acceptAgreement", IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON);
  builder->Add("eulaSystemSecuritySettings", IDS_EULA_SYSTEM_SECURITY_SETTING);

  builder->Add("eulaTpmDesc", IDS_EULA_SECURE_MODULE_DESCRIPTION);
  ::login::GetSecureModuleUsed(base::BindOnce(
      &EulaScreenHandler::UpdateLocalizedValues, weak_factory_.GetWeakPtr()));

  builder->Add("eulaSystemSecuritySettingsOkButton", IDS_OK);
  builder->Add("termsOfServiceLoading", IDS_TERMS_OF_SERVICE_SCREEN_LOADING);
#if BUILDFLAG(ENABLE_RLZ)
  builder->AddF("eulaRlzDesc",
                IDS_EULA_RLZ_DESCRIPTION,
                IDS_SHORT_PRODUCT_NAME,
                IDS_PRODUCT_NAME);
  builder->AddF("eulaRlzEnable",
                IDS_EULA_RLZ_ENABLE,
                IDS_SHORT_PRODUCT_OS_NAME);
#endif

  // Online URL to use. May be overridden by tests.
  builder->Add("eulaOnlineUrl", GetEulaOnlineUrl());
  builder->Add("eulaAdditionalToSOnlineUrl", GetAdditionalToSUrl());

  /* MD-OOBE */
  builder->Add("oobeEulaSectionTitle", IDS_OOBE_EULA_SECTION_TITLE);
  builder->Add("oobeEulaAditionalTerms", IDS_OOBE_EULA_ADDITIONAL_TERMS);
  builder->Add("oobeEulaIframeLabel", IDS_OOBE_EULA_IFRAME_LABEL);
  builder->Add("oobeEulaAcceptAndContinueButtonText",
               IDS_OOBE_EULA_ACCEPT_AND_CONTINUE_BUTTON_TEXT);
}

void EulaScreenHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
#if BUILDFLAG(ENABLE_RLZ)
  dict->SetString("rlzEnabled", "enabled");
#else
  dict->SetString("rlzEnabled", "disabled");
#endif
}

void EulaScreenHandler::Initialize() {
  if (!page_is_ready() || !screen_)
    return;

  CallJS("login.EulaScreen.setUsageStats", screen_->IsUsageStatsEnabled());

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EulaScreenHandler::OnPasswordFetched(const std::string& tpm_password) {
  CallJS("login.EulaScreen.showSecuritySettingsDialog");
}

void EulaScreenHandler::ShowStatsUsageLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_STATS_USAGE);
}

void EulaScreenHandler::ShowAdditionalTosDialog() {
  CallJS("login.EulaScreen.showAdditionalTosDialog");
}

void EulaScreenHandler::UpdateLocalizedValues(
    ::login::SecureModuleUsed secure_module_used) {
  base::DictionaryValue updated_secure_module_strings;
  auto builder = std::make_unique<::login::LocalizedValuesBuilder>(
      &updated_secure_module_strings);
  if (secure_module_used == ::login::SecureModuleUsed::TPM) {
    builder->Add("eulaTpmDesc", IDS_EULA_TPM_DESCRIPTION);
    core_oobe_view_->ReloadEulaContent(updated_secure_module_strings);
  }
}

}  // namespace chromeos
