// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_investigator.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void LogHistogramValue(signin_metrics::AccessPointAction action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AllAccessPointActions", action,
                            signin_metrics::HISTOGRAM_MAX);
}

// Returns true if |profile| is a system profile or created from one.
bool IsSystemProfile(Profile* profile) {
  return profile->GetOriginalProfile()->IsSystemProfile();
}

void RedirectToNtpOrAppsPage(content::WebContents* contents,
                             signin_metrics::AccessPoint access_point) {
  // Do nothing if a navigation is pending, since this call can be triggered
  // from DidStartLoading. This avoids deleting the pending entry while we are
  // still navigating to it. See crbug/346632.
  if (contents->GetController().GetPendingEntry())
    return;

  VLOG(1) << "RedirectToNtpOrAppsPage";
  // Redirect to NTP/Apps page and display a confirmation bubble
  GURL url(access_point ==
                   signin_metrics::AccessPoint::ACCESS_POINT_APPS_PAGE_LINK
               ? chrome::kChromeUIAppsURL
               : chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  contents->OpenURL(params);
}

void RedirectToNtpOrAppsPageIfNecessary(
    content::WebContents* contents,
    signin_metrics::AccessPoint access_point) {
  if (access_point != signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS)
    RedirectToNtpOrAppsPage(contents, access_point);
}

void CloseModalSigninIfNeeded(InlineLoginHandlerImpl* handler) {
  if (handler) {
    Browser* browser = handler->GetDesktopBrowser();
    if (browser)
      browser->signin_view_controller()->CloseModalSignin();
  }
}

void UnlockProfileAndHideLoginUI(const base::FilePath profile_path,
                                 InlineLoginHandlerImpl* handler) {
  if (!profile_path.empty()) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    if (profile_manager) {
      ProfileAttributesEntry* entry;
      if (profile_manager->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path, &entry)) {
        entry->SetIsSigninRequired(false);
      }
    }
  }
  if (handler)
    handler->CloseDialogFromJavascript();

  UserManager::Hide();
}

// Returns true if the showAccountManagement parameter in the given url is set
// to true.
bool ShouldShowAccountManagement(const GURL& url, bool is_mirror_enabled) {
  if (!is_mirror_enabled)
    return false;

  std::string value;
  if (net::GetValueForKeyInQuery(url, kSignInPromoQueryKeyShowAccountManagement,
                                 &value)) {
    int enabled = 0;
    if (base::StringToInt(value, &enabled) && enabled == 1)
      return true;
  }
  return false;
}

}  // namespace

InlineSigninHelper::InlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    Profile::CreateStatus create_status,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool choose_what_to_sync,
    bool confirm_untrusted_signin,
    bool is_force_sign_in_with_usermanager)
    : gaia_auth_fetcher_(this,
                         GaiaConstants::kChromeSource,
                         url_loader_factory),
      handler_(handler),
      profile_(profile),
      create_status_(create_status),
      current_url_(current_url),
      email_(email),
      gaia_id_(gaia_id),
      password_(password),
      auth_code_(auth_code),
      choose_what_to_sync_(choose_what_to_sync),
      confirm_untrusted_signin_(confirm_untrusted_signin),
      is_force_sign_in_with_usermanager_(is_force_sign_in_with_usermanager) {
  DCHECK(profile_);
  DCHECK(!email_.empty());
  DCHECK(!auth_code_.empty());

  gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code_, signin_scoped_device_id);
}

InlineSigninHelper::~InlineSigninHelper() {}

void InlineSigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  if (is_force_sign_in_with_usermanager_) {
    // If user sign in in UserManager with force sign in enabled, the browser
    // window won't be opened until now.
    profiles::OpenBrowserWindowForProfile(
        base::Bind(&InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened,
                   base::Unretained(this), result),
        true, false, profile_, create_status_);
  } else {
    OnClientOAuthSuccessAndBrowserOpened(result, profile_, create_status_);
  }
}

void InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened(
    const ClientOAuthResult& result,
    Profile* profile,
    Profile::CreateStatus status) {
  if (is_force_sign_in_with_usermanager_)
    UnlockProfileAndHideLoginUI(profile_->GetPath(), handler_.get());
  Browser* browser = NULL;
  if (handler_) {
    browser = handler_->GetDesktopBrowser();
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Successful");

  // Prime the account tracker with this combination of gaia id/display email.
  std::string account_id =
      AccountTrackerServiceFactory::GetForProfile(profile_)
          ->SeedAccountInfo(gaia_id_, email_);

  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForPromoURL(current_url_);
  signin_metrics::Reason reason =
      signin::GetSigninReasonForPromoURL(current_url_);

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  std::string primary_email =
      signin_manager->GetAuthenticatedAccountInfo().email;
  if (gaia::AreEmailsSame(email_, primary_email) &&
      (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
       reason == signin_metrics::Reason::REASON_UNLOCK) &&
      !password_.empty() && profiles::IsLockAvailable(profile_)) {
    LocalAuth::SetLocalAuthCredentials(profile_, password_);
  }

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  if (!password_.empty()) {
    scoped_refptr<password_manager::PasswordStore> password_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS);
    if (password_store && !primary_email.empty()) {
      password_store->SaveGaiaPasswordHash(
          primary_email, base::UTF8ToUTF16(password_),
          password_manager::metrics_util::SyncPasswordHashChange::
              SAVED_ON_CHROME_SIGNIN);
    }
  }
#endif

  if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
      reason == signin_metrics::Reason::REASON_UNLOCK ||
      reason == signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT) {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
        UpdateCredentials(account_id, result.refresh_token);

    if (signin::IsAutoCloseEnabledInURL(current_url_)) {
      // Close the gaia sign in tab via a task to make sure we aren't in the
      // middle of any webui handler code.
      bool show_account_management = ShouldShowAccountManagement(
          current_url_,
          AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile_));
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&InlineLoginHandlerImpl::CloseTab, handler_,
                                    show_account_management));
    }

    if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
        reason == signin_metrics::Reason::REASON_UNLOCK) {
      signin_manager->MergeSigninCredentialIntoCookieJar();
    }
    LogSigninReason(reason);
  } else {
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(profile_);

    OneClickSigninSyncStarter::StartSyncMode start_mode =
        OneClickSigninSyncStarter::CONFIRM_SYNC_SETTINGS_FIRST;
    if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS ||
        choose_what_to_sync_) {
      bool show_settings_without_configure =
          error_controller->HasError() && sync_service &&
          sync_service->IsFirstSetupComplete();
      if (!show_settings_without_configure)
        start_mode = OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST;
    }

    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required =
        confirm_untrusted_signin_ ?
            OneClickSigninSyncStarter::CONFIRM_UNTRUSTED_SIGNIN :
            OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN;

    bool start_signin = !HandleCrossAccountError(
        result.refresh_token, confirmation_required, start_mode);
    if (start_signin) {
      CreateSyncStarter(browser, current_url_, result.refresh_token,
                        OneClickSigninSyncStarter::CURRENT_PROFILE, start_mode,
                        confirmation_required);
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    }
  }
}

void InlineSigninHelper::CreateSyncStarter(
    Browser* browser,
    const GURL& current_url,
    const std::string& refresh_token,
    OneClickSigninSyncStarter::ProfileMode profile_mode,
    OneClickSigninSyncStarter::StartSyncMode start_mode,
    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required) {
  // OneClickSigninSyncStarter will delete itself once the job is done.
  new OneClickSigninSyncStarter(
      profile_, browser, gaia_id_, email_, password_, refresh_token,
      signin::GetAccessPointForPromoURL(current_url),
      signin::GetSigninReasonForPromoURL(current_url), profile_mode, start_mode,
      confirmation_required,
      base::Bind(&InlineLoginHandlerImpl::SyncStarterCallback, handler_));
}

bool InlineSigninHelper::HandleCrossAccountError(
    const std::string& refresh_token,
    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required,
    OneClickSigninSyncStarter::StartSyncMode start_mode) {
  // With force sign in enabled, cross account
  // sign in will be rejected in the early stage so there is no need to show the
  // warning page here.
  if (signin_util::IsForceSigninEnabled())
    return false;

  std::string last_email =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);

  // TODO(skym): Warn for high risk upgrade scenario, crbug.com/572754.
  if (!IsCrossAccountError(profile_, email_, gaia_id_))
    return false;

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  SigninEmailConfirmationDialog::AskForConfirmation(
      web_contents, profile_, last_email, email_,
      base::Bind(&InlineSigninHelper::ConfirmEmailAction,
                 base::Unretained(this), web_contents, refresh_token,
                 confirmation_required, start_mode));
  return true;
}

void InlineSigninHelper::ConfirmEmailAction(
    content::WebContents* web_contents,
    const std::string& refresh_token,
    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required,
    OneClickSigninSyncStarter::StartSyncMode start_mode,
    SigninEmailConfirmationDialog::Action action) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  switch (action) {
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
      CreateSyncStarter(browser, current_url_, refresh_token,
                        OneClickSigninSyncStarter::NEW_PROFILE, start_mode,
                        confirmation_required);
      break;
    case SigninEmailConfirmationDialog::START_SYNC:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
      CreateSyncStarter(browser, current_url_, refresh_token,
                        OneClickSigninSyncStarter::CURRENT_PROFILE, start_mode,
                        confirmation_required);
      break;
    case SigninEmailConfirmationDialog::CLOSE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
      if (handler_) {
        handler_->SyncStarterCallback(
            OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
      }
      break;
    default:
      DCHECK(false) << "Invalid action";
  }
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void InlineSigninHelper::OnClientOAuthFailure(
  const GoogleServiceAuthError& error) {
  if (handler_)
    handler_->HandleLoginError(error.ToString(), base::string16());

  AboutSigninInternals* about_signin_internals =
    AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Failure");

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

InlineLoginHandlerImpl::InlineLoginHandlerImpl()
      : confirm_untrusted_signin_(false),
        weak_factory_(this) {
}

InlineLoginHandlerImpl::~InlineLoginHandlerImpl() {}

// This method is not called with webview sign in enabled.
void InlineLoginHandlerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!web_contents() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

  // Returns early if this is not a gaia webview navigation.
  content::RenderFrameHost* gaia_frame =
      signin::GetAuthFrame(web_contents(), "signin-frame");
  if (navigation_handle->GetRenderFrameHost() != gaia_frame)
    return;

  // Loading any untrusted (e.g., HTTP) URLs in the privileged sign-in process
  // will require confirmation before the sign in takes effect.
  const GURL kGaiaExtOrigin(
      GaiaUrls::GetInstance()->signin_completed_continue_url().GetOrigin());
  if (!navigation_handle->GetURL().is_empty()) {
    GURL origin(navigation_handle->GetURL().GetOrigin());
    if (navigation_handle->GetURL().spec() != url::kAboutBlankURL &&
        origin != kGaiaExtOrigin &&
        !gaia::IsGaiaSignonRealm(origin)) {
      confirm_untrusted_signin_ = true;
    }
  }
}

// static
void InlineLoginHandlerImpl::SetExtraInitParams(base::DictionaryValue& params) {
  params.SetString("service", "chromiumsync");

  // If this was called from the user manager to reauthenticate the profile,
  // make sure the webui is aware.
  Profile* profile = Profile::FromWebUI(web_ui());
  if (IsSystemProfile(profile))
    params.SetBoolean("dontResizeNonEmbeddedPages", true);

  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();
  signin_metrics::Reason reason =
      signin::GetSigninReasonForPromoURL(current_url);

    const GURL& url = GaiaUrls::GetInstance()->embedded_signin_url();
    params.SetBoolean("isNewGaiaFlow", true);
    params.SetString("clientId",
                     GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    params.SetString("gaiaPath", url.path().substr(1));

    std::string flow;
    switch (reason) {
      case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
        flow = "addaccount";
        break;
      case signin_metrics::Reason::REASON_REAUTHENTICATION:
      case signin_metrics::Reason::REASON_UNLOCK:
        flow = "reauth";
        break;
      case signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT:
        flow = "enterprisefsi";
        break;
      default:
        flow = "signin";
        break;
    }
    params.SetString("flow", flow);

  content::WebContentsObserver::Observe(contents);
  LogHistogramValue(signin_metrics::HISTOGRAM_SHOWN);
}

void InlineLoginHandlerImpl::CompleteLogin(const std::string& email,
                                           const std::string& password,
                                           const std::string& gaia_id,
                                           const std::string& auth_code,
                                           bool skip_for_now,
                                           bool trusted,
                                           bool trusted_found,
                                           bool choose_what_to_sync) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();

  if (skip_for_now) {
    signin::SetUserSkippedPromo(Profile::FromWebUI(web_ui()));
    SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
    return;
  }

  // This value exists only for webview sign in.
  if (trusted_found)
    confirm_untrusted_signin_ = !trusted;

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());
  DCHECK(!auth_code.empty());

  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          contents->GetBrowserContext(), signin::GetSigninPartitionURL());

  // If this was called from the user manager to reauthenticate the profile,
  // the current profile is the system profile.  In this case, use the email to
  // find the right profile to reauthenticate.  Otherwise the profile can be
  // taken from web_ui().
  Profile* profile = Profile::FromWebUI(web_ui());
  if (IsSystemProfile(profile)) {
    ProfileManager* manager = g_browser_process->profile_manager();
    base::FilePath path = profiles::GetPathOfProfileWithEmail(manager, email);
    if (path.empty()) {
      path = UserManager::GetSigninProfilePath();
    }
    if (!path.empty()) {
      signin_metrics::Reason reason =
          signin::GetSigninReasonForPromoURL(current_url);
      // If we are only reauthenticating a profile in the user manager (and not
      // unlocking it), load the profile and finish the login.
      if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION) {
        FinishCompleteLoginParams params(
            this, partition, current_url, base::FilePath(),
            confirm_untrusted_signin_, email, gaia_id, password, auth_code,
            choose_what_to_sync, false);
        ProfileManager::CreateCallback callback =
            base::Bind(&InlineLoginHandlerImpl::FinishCompleteLogin, params);
        profiles::LoadProfileAsync(path, callback);
      } else {
        // Otherwise, switch to the profile and finish the login. Pass the
        // profile path so it can be marked as unlocked. Don't pass a handler
        // pointer since it will be destroyed before the callback runs.
        bool is_force_signin_enabled = signin_util::IsForceSigninEnabled();
        InlineLoginHandlerImpl* handler = nullptr;
        if (is_force_signin_enabled)
          handler = this;
        FinishCompleteLoginParams params(
            handler, partition, current_url, path, confirm_untrusted_signin_,
            email, gaia_id, password, auth_code, choose_what_to_sync,
            is_force_signin_enabled);
        ProfileManager::CreateCallback callback =
            base::Bind(&InlineLoginHandlerImpl::FinishCompleteLogin, params);
        if (is_force_signin_enabled) {
          // Browser window will be opened after ClientOAuthSuccess.
          profiles::LoadProfileAsync(path, callback);
        } else {
          profiles::SwitchToProfile(path, true, callback,
                                    ProfileMetrics::SWITCH_PROFILE_UNLOCK);
        }
      }
    }
  } else {
    FinishCompleteLogin(FinishCompleteLoginParams(
                            this, partition, current_url, base::FilePath(),
                            confirm_untrusted_signin_, email, gaia_id, password,
                            auth_code, choose_what_to_sync, false),
                        profile, Profile::CREATE_STATUS_CREATED);
  }
}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    InlineLoginHandlerImpl* handler,
    content::StoragePartition* partition,
    const GURL& url,
    const base::FilePath& profile_path,
    bool confirm_untrusted_signin,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    bool choose_what_to_sync,
    bool is_force_sign_in_with_usermanager)
    : handler(handler),
      partition(partition),
      url(url),
      profile_path(profile_path),
      confirm_untrusted_signin(confirm_untrusted_signin),
      email(email),
      gaia_id(gaia_id),
      password(password),
      auth_code(auth_code),
      choose_what_to_sync(choose_what_to_sync),
      is_force_sign_in_with_usermanager(is_force_sign_in_with_usermanager) {}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    const FinishCompleteLoginParams& other) = default;

InlineLoginHandlerImpl::
    FinishCompleteLoginParams::~FinishCompleteLoginParams() {}

// static
void InlineLoginHandlerImpl::FinishCompleteLogin(
    const FinishCompleteLoginParams& params,
    Profile* profile,
    Profile::CreateStatus status) {
  // When doing a SAML sign in, this email check may result in a false
  // positive.  This happens when the user types one email address in the
  // gaia sign in page, but signs in to a different account in the SAML sign in
  // page.
  std::string default_email;
  std::string validate_email;
  if (net::GetValueForKeyInQuery(params.url, "email", &default_email) &&
      net::GetValueForKeyInQuery(params.url, "validateEmail",
                                 &validate_email) &&
      validate_email == "1" && !default_email.empty()) {
    if (!gaia::AreEmailsSame(params.email, default_email)) {
      if (params.handler) {
        params.handler->HandleLoginError(
            l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      base::UTF8ToUTF16(default_email)),
            base::UTF8ToUTF16(params.email));
      }
      return;
    }
  }

  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForPromoURL(params.url);
  signin_metrics::Reason reason =
      signin::GetSigninReasonForPromoURL(params.url);
  LogHistogramValue(signin_metrics::HISTOGRAM_ACCEPTED);
  bool switch_to_advanced =
      params.choose_what_to_sync &&
      (access_point != signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  LogHistogramValue(
      switch_to_advanced ? signin_metrics::HISTOGRAM_WITH_ADVANCED :
                           signin_metrics::HISTOGRAM_WITH_DEFAULTS);

  CanOfferSigninType can_offer_for = CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS;
  switch (reason) {
    case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
      can_offer_for = CAN_OFFER_SIGNIN_FOR_SECONDARY_ACCOUNT;
      break;
    case signin_metrics::Reason::REASON_REAUTHENTICATION:
    case signin_metrics::Reason::REASON_UNLOCK: {
      std::string primary_username =
          SigninManagerFactory::GetForProfile(profile)
              ->GetAuthenticatedAccountInfo()
              .email;
      if (!gaia::AreEmailsSame(default_email, primary_username))
        can_offer_for = CAN_OFFER_SIGNIN_FOR_SECONDARY_ACCOUNT;
      break;
    }
    default:
      // No need to change |can_offer_for|.
      break;
  }

  std::string error_msg;
  bool can_offer = CanOfferSignin(profile, can_offer_for, params.gaia_id,
                                  params.email, &error_msg);
  if (!can_offer) {
    if (params.handler) {
      params.handler->HandleLoginError(error_msg,
                                       base::UTF8ToUTF16(params.email));
    }
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  about_signin_internals->OnAuthenticationResultReceived("Successful");

  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(profile);
  base::WeakPtr<InlineLoginHandlerImpl> handler_weak_ptr;
  if (params.handler)
    handler_weak_ptr = params.handler->GetWeakPtr();

  // InlineSigninHelper will delete itself.
  new InlineSigninHelper(
      handler_weak_ptr,
      params.partition->GetURLLoaderFactoryForBrowserProcess(), profile, status,
      params.url, params.email, params.gaia_id, params.password,
      params.auth_code, signin_scoped_device_id, params.choose_what_to_sync,
      params.confirm_untrusted_signin,
      params.is_force_sign_in_with_usermanager);

  // If opened from user manager to unlock a profile, make sure the user manager
  // is closed and that the profile is marked as unlocked.
  if (!params.is_force_sign_in_with_usermanager) {
    UnlockProfileAndHideLoginUI(params.profile_path, params.handler);
  }
}

void InlineLoginHandlerImpl::HandleLoginError(const std::string& error_msg,
                                              const base::string16& email) {
  SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
  Browser* browser = GetDesktopBrowser();
  Profile* profile = Profile::FromWebUI(web_ui());

  if (IsSystemProfile(profile))
    profile = g_browser_process->profile_manager()->GetProfileByPath(
        UserManager::GetSigninProfilePath());
  CloseModalSigninIfNeeded(this);
  if (!error_msg.empty()) {
    LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(
        browser, base::UTF8ToUTF16(error_msg), email);
  }
}

Browser* InlineLoginHandlerImpl::GetDesktopBrowser() {
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  if (!browser)
    browser = chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui()));
  return browser;
}

void InlineLoginHandlerImpl::SyncStarterCallback(
    OneClickSigninSyncStarter::SyncSetupResult result) {
  content::WebContents* contents = web_ui()->GetWebContents();

  if (contents->GetController().GetPendingEntry()) {
    // Do nothing if a navigation is pending, since this call can be triggered
    // from DidStartLoading. This avoids deleting the pending entry while we are
    // still navigating to it. See crbug/346632.
    return;
  }

  const GURL& current_url = contents->GetLastCommittedURL();
  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForPromoURL(current_url);
  bool auto_close = signin::IsAutoCloseEnabledInURL(current_url);

  if (result == OneClickSigninSyncStarter::SYNC_SETUP_FAILURE) {
    RedirectToNtpOrAppsPage(contents, access_point);
  } else if (auto_close) {
    bool show_account_management = ShouldShowAccountManagement(
        current_url, AccountConsistencyModeManager::IsMirrorEnabledForProfile(
                         Profile::FromWebUI(web_ui())));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&InlineLoginHandlerImpl::CloseTab,
                       weak_factory_.GetWeakPtr(), show_account_management));
  } else {
    RedirectToNtpOrAppsPageIfNecessary(contents, access_point);
  }
}

void InlineLoginHandlerImpl::CloseTab(bool show_account_management) {
  content::WebContents* tab = web_ui()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(tab);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model) {
      int index = tab_strip_model->GetIndexOfWebContents(tab);
      if (index != TabStripModel::kNoTab) {
        tab_strip_model->ExecuteContextMenuCommand(
            index, TabStripModel::CommandCloseTab);
      }
    }

    if (show_account_management) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT,
          signin::ManageAccountsParams(),
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
          false);
    }
  }
}
