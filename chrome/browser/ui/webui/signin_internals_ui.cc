// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_internals_ui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/hash/hash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

content::WebUIDataSource* CreateSignInInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISignInInternalsHost);
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
  return source;
}

}  //  namespace

SignInInternalsUI::SignInInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateSignInInternalsHTMLSource());
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
      about_signin_internals->AddSigninObserver(this);
  }
}

void SignInInternalsHandler::OnJavascriptDisallowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile);
    if (about_signin_internals) {
      about_signin_internals->RemoveSigninObserver(this);
    }
  }
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
  if (!about_signin_internals) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  // TODO(vishwath): The UI would look better if we passed in a dict with some
  // reasonable defaults, so the about:signin-internals page doesn't look
  // empty in incognito mode. Alternatively, we could force about:signin to
  // open in non-incognito mode always (like about:settings for ex.).
  ResolveJavascriptCallback(base::Value(callback_id),
                            about_signin_internals->GetSigninStatus());
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
  FireWebUIListener("signin-info-changed", info);
}

void SignInInternalsHandler::OnCookieAccountsFetched(
    const base::Value::Dict& info) {
  FireWebUIListener("update-cookie-accounts", info);
}
