// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler.h"

#include <limits.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

const char kSignInPromoQueryKeyShowAccountManagement[] =
    "showAccountManagement";

InlineLoginHandler::InlineLoginHandler() {}

InlineLoginHandler::~InlineLoginHandler() {}

void InlineLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&InlineLoginHandler::HandleInitializeMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "authExtensionReady",
      base::BindRepeating(&InlineLoginHandler::HandleAuthExtensionReadyMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "completeLogin",
      base::BindRepeating(&InlineLoginHandler::HandleCompleteLoginMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "switchToFullTab",
      base::BindRepeating(&InlineLoginHandler::HandleSwitchToFullTabMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "navigationButtonClicked",
      base::BindRepeating(&InlineLoginHandler::HandleNavigationButtonClicked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dialogClose", base::BindRepeating(&InlineLoginHandler::HandleDialogClose,
                                         base::Unretained(this)));
}

void InlineLoginHandler::HandleInitializeMessage(const base::ListValue* args) {
  AllowJavascript();
  content::WebContents* contents = web_ui()->GetWebContents();
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          contents->GetBrowserContext(), signin::GetSigninPartitionURL());
  if (partition) {
    const GURL& current_url = web_ui()->GetWebContents()->GetURL();

    // If the kSignInPromoQueryKeyForceKeepData param is missing, or if it is
    // present and its value is zero, this means we don't want to keep the
    // the data.
    std::string value;
    if (!net::GetValueForKeyInQuery(current_url,
                                    signin::kSignInPromoQueryKeyForceKeepData,
                                    &value) ||
        value == "0") {
      partition->ClearData(
          content::StoragePartition::REMOVE_DATA_MASK_ALL,
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
          GURL(),
          base::Time(),
          base::Time::Max(),
          base::Bind(&InlineLoginHandler::ContinueHandleInitializeMessage,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      ContinueHandleInitializeMessage();
    }
  }
}

void InlineLoginHandler::ContinueHandleInitializeMessage() {
  base::DictionaryValue params;

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  params.SetString("hl", app_locale);
  GaiaUrls* gaiaUrls = GaiaUrls::GetInstance();
  params.SetString("gaiaUrl", gaiaUrls->gaia_url().spec());
  params.SetInteger("authMode", InlineLoginHandler::kDesktopAuthMode);

  const GURL& current_url = web_ui()->GetWebContents()->GetURL();
  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForEmbeddedPromoURL(current_url);
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(current_url);

  if (reason != signin_metrics::Reason::REASON_REAUTHENTICATION &&
      reason != signin_metrics::Reason::REASON_UNLOCK &&
      reason != signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT) {
    signin_metrics::LogSigninAccessPointStarted(
        access_point,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    signin_metrics::RecordSigninUserActionForAccessPoint(
        access_point,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Loading"));
    params.SetBoolean("isLoginPrimaryAccount", true);
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  std::string default_email;
  if (reason == signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT ||
      reason == signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT) {
    default_email =
        profile->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
  } else {
    if (!net::GetValueForKeyInQuery(current_url, "email", &default_email))
      default_email.clear();
  }
  if (!default_email.empty())
    params.SetString("email", default_email);

  // The legacy full-tab Chrome sign-in page is no longer used as it was relying
  // on exchanging cookies for refresh tokens and that endpoint is no longer
  // supported.
  params.SetString("constrained", "1");

  // TODO(rogerta): this needs to be passed on to gaia somehow.
  std::string read_only_email;
  net::GetValueForKeyInQuery(current_url, "readOnlyEmail", &read_only_email);
  params.SetBoolean("readOnlyEmail", !read_only_email.empty());

  SetExtraInitParams(params);
  CallJavascriptFunction("inline.login.loadAuthExtension", params);
}

void InlineLoginHandler::HandleCompleteLoginMessage(
    const base::ListValue* args) {
  // When the network service is enabled, the webRequest API doesn't expose
  // cookie headers. So manually fetch the cookies for the GAIA URL from the
  // CookieManager.
  content::WebContents* contents = web_ui()->GetWebContents();
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          contents->GetBrowserContext(), signin::GetSigninPartitionURL());

  partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      GaiaUrls::GetInstance()->gaia_url(),
      net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&InlineLoginHandler::HandleCompleteLoginMessageWithCookies,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::ListValue(args->GetList())));
}

void InlineLoginHandler::HandleCompleteLoginMessageWithCookies(
    const base::ListValue& args,
    const net::CookieStatusList& cookies,
    const net::CookieStatusList& excluded_cookies) {
  const base::DictionaryValue* dict = nullptr;
  args.GetDictionary(0, &dict);

  const std::string& email = dict->FindKey("email")->GetString();
  const std::string& password = dict->FindKey("password")->GetString();
  const std::string& gaia_id = dict->FindKey("gaiaId")->GetString();

  std::string auth_code;
  for (const auto& cookie_with_status : cookies) {
    if (cookie_with_status.cookie.Name() == "oauth_code")
      auth_code = cookie_with_status.cookie.Value();
  }

  bool skip_for_now = false;
  dict->GetBoolean("skipForNow", &skip_for_now);
  bool trusted = false;
  bool trusted_found = dict->GetBoolean("trusted", &trusted);

  bool choose_what_to_sync = false;
  dict->GetBoolean("chooseWhatToSync", &choose_what_to_sync);

  CompleteLogin(email, password, gaia_id, auth_code, skip_for_now, trusted,
                trusted_found, choose_what_to_sync);
}

void InlineLoginHandler::HandleSwitchToFullTabMessage(
    const base::ListValue* args) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  if (browser) {
    // |web_ui| is already presented in a full tab. Ignore this call.
    return;
  }

  std::string url_str;
  CHECK(args->GetString(0, &url_str));

  Profile* profile = Profile::FromWebUI(web_ui());
  GURL main_frame_url(web_ui()->GetWebContents()->GetURL());

  // Adds extra parameters to the signin URL so that Chrome will close the tab
  // and show the account management view of the avatar menu upon completion.
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, signin::kSignInPromoQueryKeyAutoClose, "1");
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, kSignInPromoQueryKeyShowAccountManagement, "1");
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, signin::kSignInPromoQueryKeyForceKeepData, "1");

  NavigateParams params(profile, main_frame_url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Navigate(&params);

  CloseDialogFromJavascript();
}

void InlineLoginHandler::HandleNavigationButtonClicked(
    const base::ListValue* args) {
#if !defined(OS_CHROMEOS)
  NOTREACHED() << "The inline login handler is no longer used in a browser "
                  "or tab modal dialog.";
#else
  FireWebUIListener("navigateBackInWebview");
#endif
}

void InlineLoginHandler::HandleDialogClose(const base::ListValue* args) {
#if !defined(OS_CHROMEOS)
  // Does nothing if user manager is not showing.
  UserManagerProfileDialog::HideDialog();
#endif  // !defined(OS_CHROMEOS)
}

void InlineLoginHandler::CloseDialogFromJavascript() {
  if (IsJavascriptAllowed())
    CallJavascriptFunction("inline.login.closeDialog");
}
