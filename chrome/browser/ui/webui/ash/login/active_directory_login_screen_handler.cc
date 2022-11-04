// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/active_directory_login_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/active_directory_login_screen.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

ActiveDirectoryLoginScreenHandler::ActiveDirectoryLoginScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ActiveDirectoryLoginScreenHandler::~ActiveDirectoryLoginScreenHandler() =
    default;

void ActiveDirectoryLoginScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("adAuthWelcomeMessage", IDS_AD_DOMAIN_AUTH_WELCOME_MESSAGE);
  builder->Add("adAuthLoginUsername", IDS_AD_AUTH_LOGIN_USER);
  builder->Add("adLoginPassword", IDS_AD_LOGIN_PASSWORD);
  builder->AddF("loginWelcomeMessage", IDS_LOGIN_WELCOME_MESSAGE,
                ui::GetChromeOSDeviceTypeResourceId());
}

void ActiveDirectoryLoginScreenHandler::Show() {
  base::Value::Dict screen_data;
  screen_data.Set("realm", g_browser_process->platform_part()
                               ->browser_policy_connector_ash()
                               ->GetRealm());
  std::string email_domain;
  if (CrosSettings::Get()->GetString(kAccountsPrefLoginScreenDomainAutoComplete,
                                     &email_domain) &&
      !email_domain.empty()) {
    screen_data.Set("emailDomain", email_domain);
  }

  ShowInWebUI(std::move(screen_data));
}

void ActiveDirectoryLoginScreenHandler::Reset() {
  CallExternalAPI("reset");
}

void ActiveDirectoryLoginScreenHandler::SetErrorState(
    const std::string& username,
    int errorState) {
  CallExternalAPI("setErrorState", username, errorState);
}

}  // namespace ash
