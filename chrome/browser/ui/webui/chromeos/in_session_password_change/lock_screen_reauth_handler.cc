// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_handler.h"

#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/saml/public_saml_url_fetcher.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"

namespace chromeos {

LockScreenReauthHandler::LockScreenReauthHandler(const std::string& email)
    : email_(email) {}
LockScreenReauthHandler::~LockScreenReauthHandler() = default;

void LockScreenReauthHandler::HandleInitialize(const base::ListValue* value) {
  LoadAuthenticatorParam();
}

void LockScreenReauthHandler::HandleAuthenticatorLoaded(
    const base::ListValue*) {
  VLOG(1) << "Authenticator finished loading";
  authenticator_being_loaded_ = false;
  // Recreate the client cert usage observer, in order to track only the certs
  // used during the current sign-in attempt.
  extension_provided_client_cert_usage_observer_ =
      std::make_unique<LoginClientCertUsageObserver>();
}

void LockScreenReauthHandler::LoadAuthenticatorParam() {
  if (authenticator_being_loaded_) {
    VLOG(1) << "Skip loading the Authenticator as it's already being loaded ";
    return;
  }

  authenticator_being_loaded_ = true;
  login::GaiaContext context;
  context.force_reload = true;
  context.email = email_;

  std::string gaia_id;
  if (!context.email.empty() &&
      user_manager::known_user::FindGaiaID(
          AccountId::FromUserEmail(context.email), &gaia_id)) {
    context.gaia_id = gaia_id;
  }

  if (!context.email.empty()) {
    context.gaps_cookie = user_manager::known_user::GetGAPSCookie(
        AccountId::FromUserEmail(gaia::CanonicalizeEmail(context.email)));
  }

  LoadGaia(context);
}

void LockScreenReauthHandler::LoadGaia(const login::GaiaContext& context) {
  // Start a new session with SigninPartitionManager, generating a unique
  // StoragePartition.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  signin_partition_manager->StartSigninSession(
      web_ui()->GetWebContents(),
      base::BindOnce(&LockScreenReauthHandler::LoadGaiaWithPartition,
                     weak_factory_.GetWeakPtr(), context));
}

void LockScreenReauthHandler::LoadGaiaWithPartition(
    const login::GaiaContext& context,
    const std::string& partition_name) {
  auto callback = base::BindOnce(
      &LockScreenReauthHandler::OnSetCookieForLoadGaiaWithPartition,
      weak_factory_.GetWeakPtr(), context, partition_name);
  if (context.gaps_cookie.empty()) {
    std::move(callback).Run(net::CookieAccessResult());
    return;
  }

  // When the network service is enabled the webRequest API doesn't allow
  // modification of the cookie header. So manually write the GAPS cookie into
  // the CookieManager.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  login::SetCookieForPartition(context, signin_partition_manager,
                               std::move(callback));
}

void LockScreenReauthHandler::OnSetCookieForLoadGaiaWithPartition(
    const login::GaiaContext& context,
    const std::string& partition_name,
    net::CookieAccessResult result) {
  base::DictionaryValue params;

  params.SetString("webviewPartitionName", partition_name);
  params.SetString("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  params.SetString("clientId",
                   GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.SetBoolean("dontResizeNonEmbeddedPages", false);
  params.SetBoolean("enableGaiaActionButtons", false);

  std::string enterprise_enrollment_domain(
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetEnterpriseEnrollmentDomain());

  if (enterprise_enrollment_domain.empty()) {
    enterprise_enrollment_domain = gaia::ExtractDomainName(context.email);
  }

  params.SetString("enterpriseEnrollmentDomain", enterprise_enrollment_domain);

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  DCHECK(!app_locale.empty());
  params.SetString("hl", app_locale);
  params.SetString("email", context.email);
  params.SetString("gaiaId", context.gaia_id);
  params.SetBoolean("extractSamlPasswordAttributes",
                    login::ExtractSamlPasswordAttributesEnabled());

  AllowJavascript();
  CallJavascriptFunction("$(\'main-element\').LoadAuthenticatorParam", params);
}

void LockScreenReauthHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&LockScreenReauthHandler::HandleInitialize,
                          weak_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "authenticatorLoaded",
      base::BindRepeating(&LockScreenReauthHandler::HandleAuthenticatorLoaded,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos