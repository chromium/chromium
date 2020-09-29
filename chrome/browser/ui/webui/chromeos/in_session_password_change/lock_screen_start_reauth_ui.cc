// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_start_reauth_ui.h"

#include <memory>

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

LockScreenStartReauthUI::LockScreenStartReauthUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  std::string email;
  if (user) {
    email = user->GetDisplayEmail();
  }

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUILockScreenStartReauthHost);

  web_ui->AddMessageHandler(std::make_unique<LockScreenReauthHandler>());

  // TODO(crbug.com/1098690): Trusted Type Polymer
  source->DisableTrustedTypesCSP();

  source->AddString("lockScreenReauthSubtitile",
                    l10n_util::GetStringFUTF16(IDS_LOCK_SCREEN_REAUTH_SUBTITLE,
                                               base::UTF8ToUTF16(email)));
  source->AddString("lockScreenReauthSubtitileWithError",
                    l10n_util::GetStringFUTF16(IDS_LOCK_SCREEN_WRONG_USER,
                                               base::UTF8ToUTF16(email)));

  source->AddString("lockScreenVerifyButton",
                    l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFY_BUTTON));
  source->AddString(
      "lockScreenVerifyAgainButton",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFY_AGAIN_BUTTON));
  source->AddString("lockScreenCancelButton",
                    l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_CANCEL_BUTTON));
  source->AddString("loginWelcomeMessage",
                    l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFY_ACCOUNT));
  source->AddString(
      "loginWelcomeMessageWithError",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFICATION_FAILED));

  source->SetDefaultResource(IDR_LOCK_SCREEN_REAUTH_HTML);

  source->AddResourcePath("authenticator.js", IDR_GAIA_AUTH_AUTHENTICATOR_JS);

  source->AddResourcePath("lock_screen_reauth.js", IDR_LOCK_SCREEN_REAUTH_JS);

  content::WebUIDataSource::Add(profile, source);
}

LockScreenStartReauthUI::~LockScreenStartReauthUI() = default;

}  // namespace chromeos
