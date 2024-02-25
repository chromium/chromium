// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_internals_ui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/common/renderer_configuration.mojom.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace {

void CreateAndAddSignInInternalsHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISignInInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");

  source->UseStringsJs();
  source->AddResourcePath("signin_internals.js", IDR_SIGNIN_INTERNALS_INDEX_JS);
  source->AddResourcePath("signin_index.css", IDR_SIGNIN_INTERNALS_INDEX_CSS);
  source->SetDefaultResource(IDR_SIGNIN_INTERNALS_INDEX_HTML);
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
void AppendBoundSessionInfo(
    base::Value::Dict& signin_status,
    BoundSessionCookieRefreshService* bound_session_service) {
  // TODO(b/299884315): update bound session info dynamically by observing the
  // service.
  // TODO(b/299884315): expose extra info that is not available in
  // BoundSessionThrottlerParams, like session ID or a bound cookie names.
  base::Value::Dict bound_session_entry;
  if (!bound_session_service) {
    bound_session_entry.Set("domain", "Bound session service is disabled.");
  } else if (chrome::mojom::BoundSessionThrottlerParamsPtr
                 bound_session_throttler_params =
                     bound_session_service->GetBoundSessionThrottlerParams();
             !bound_session_throttler_params) {
    bound_session_entry.Set("domain", "No active bound sessions.");
  } else {
    bound_session_entry.Set("domain", bound_session_throttler_params->domain);
    bound_session_entry.Set("path", bound_session_throttler_params->path);
    bound_session_entry.Set(
        "expirationTime",
        base::TimeFormatAsIso8601(
            bound_session_throttler_params->cookie_expiry_date));
  }
  signin_status.Set("boundSessionInfo",
                    base::Value::List().Append(std::move(bound_session_entry)));
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

}  //  namespace

SignInInternalsUI::SignInInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  CreateAndAddSignInInternalsHTMLSource(Profile::FromWebUI(web_ui));
  web_ui->AddMessageHandler(std::make_unique<SignInInternalsHandler>());
}

SignInInternalsUI::~SignInInternalsUI() = default;

SignInInternalsHandler::SignInInternalsHandler() = default;

SignInInternalsHandler::~SignInInternalsHandler() {
  // This handler can be destroyed without OnJavascriptDisallowed() ever being
  // called (https://crbug.com/1199198). Call it to ensure that `this` is
  // removed as an observer.
  OnJavascriptDisallowed();
}

void SignInInternalsHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile);
    if (about_signin_internals)
      about_signin_internals_observeration_.Observe(about_signin_internals);
  }
}

void SignInInternalsHandler::OnJavascriptDisallowed() {
  about_signin_internals_observeration_.Reset();
}

void SignInInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSigninInfo",
      base::BindRepeating(&SignInInternalsHandler::HandleGetSignInInfo,
                          base::Unretained(this)));
}

void SignInInternalsHandler::HandleGetSignInInfo(
    const base::Value::List& args) {
  std::string callback_id = args[0].GetString();
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  // TODO(vishwath): The UI would look better if we passed in a dict with some
  // reasonable defaults, so the about:signin-internals page doesn't look
  // empty in incognito mode. Alternatively, we could force about:signin to
  // open in non-incognito mode always (like about:settings for ex.).
  base::Value::Dict signin_status =
      about_signin_internals ? about_signin_internals->GetSigninStatus()
                             : base::Value::Dict();
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (switches::IsBoundSessionCredentialsEnabled()) {
    AppendBoundSessionInfo(
        signin_status,
        BoundSessionCookieRefreshServiceFactory::GetForProfile(profile));
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  ResolveJavascriptCallback(base::Value(callback_id), std::move(signin_status));

  if (!about_signin_internals) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
      identity_manager->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar.accounts_are_fresh) {
    about_signin_internals->OnAccountsInCookieUpdated(
        accounts_in_cookie_jar,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void SignInInternalsHandler::OnSigninStateChanged(
    const base::Value::Dict& info) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile && switches::IsBoundSessionCredentialsEnabled()) {
    base::Value::Dict signin_status = info.Clone();
    AppendBoundSessionInfo(
        signin_status,
        BoundSessionCookieRefreshServiceFactory::GetForProfile(profile));
    FireWebUIListener("signin-info-changed", signin_status);
    return;
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  FireWebUIListener("signin-info-changed", info);
}

void SignInInternalsHandler::OnCookieAccountsFetched(
    const base::Value::Dict& info) {
  FireWebUIListener("update-cookie-accounts", info);
}
