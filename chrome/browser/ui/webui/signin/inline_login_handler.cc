// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler.h"

#include <limits.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
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

InlineLoginHandler::InlineLoginHandler() = default;

InlineLoginHandler::~InlineLoginHandler() = default;

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
      "dialogClose", base::BindRepeating(&InlineLoginHandler::HandleDialogClose,
                                         base::Unretained(this)));
}

void InlineLoginHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void InlineLoginHandler::HandleInitializeMessage(const base::ListValue* args) {
  AllowJavascript();
  content::WebContents* contents = web_ui()->GetWebContents();
  content::StoragePartition* partition =
      signin::GetSigninPartition(contents->GetBrowserContext());
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
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
          base::Time(), base::Time::Max(),
          base::BindOnce(&InlineLoginHandler::ContinueHandleInitializeMessage,
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

  if (reason != signin_metrics::Reason::kReauthentication &&
      reason != signin_metrics::Reason::kAddSecondaryAccount) {
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
  if (reason == signin_metrics::Reason::kSigninPrimaryAccount ||
      reason == signin_metrics::Reason::kForcedSigninPrimaryAccount) {
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
  FireWebUIListener("load-auth-extension", params);
}

void InlineLoginHandler::HandleCompleteLoginMessage(
    const base::ListValue* args) {
  // When the network service is enabled, the webRequest API doesn't expose
  // cookie headers. So manually fetch the cookies for the GAIA URL from the
  // CookieManager.
  content::WebContents* contents = web_ui()->GetWebContents();
  content::StoragePartition* partition =
      signin::GetSigninPartition(contents->GetBrowserContext());

  partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      GaiaUrls::GetInstance()->gaia_url(),
      net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&InlineLoginHandler::HandleCompleteLoginMessageWithCookies,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::ListValue(args->GetList())));
}

void InlineLoginHandler::HandleCompleteLoginMessageWithCookies(
    const base::ListValue& args,
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  const base::Value& dict = args.GetList()[0];

  const std::string& email = dict.FindKey("email")->GetString();
  const std::string& password = dict.FindKey("password")->GetString();
  const std::string& gaia_id = dict.FindKey("gaiaId")->GetString();

  std::string auth_code;
  for (const auto& cookie_with_access_result : cookies) {
    if (cookie_with_access_result.cookie.Name() == "oauth_code")
      auth_code = cookie_with_access_result.cookie.Value();
  }

  bool skip_for_now = dict.FindBoolKey("skipForNow").value_or(false);
  base::Optional<bool> trusted = dict.FindBoolKey("trusted");
  bool trusted_value = trusted.value_or(false);
  bool trusted_found = trusted.has_value();

  bool choose_what_to_sync =
      dict.FindBoolKey("chooseWhatToSync").value_or(false);

  base::Value edu_login_params;
  if (args.GetList().size() > 1) {
    edu_login_params = args.GetList()[1].Clone();
  }

  CompleteLogin(email, password, gaia_id, auth_code, skip_for_now,
                trusted_value, trusted_found, choose_what_to_sync,
                std::move(edu_login_params));
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

void InlineLoginHandler::HandleDialogClose(const base::ListValue* args) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Does nothing if profile picker is not showing.
  ProfilePickerForceSigninDialog::HideDialog();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void InlineLoginHandler::CloseDialogFromJavascript() {
  if (IsJavascriptAllowed())
    FireWebUIListener("close-dialog");
}
