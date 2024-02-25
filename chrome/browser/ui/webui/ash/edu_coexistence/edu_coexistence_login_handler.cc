// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/edu_coexistence_tos_store_utils.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_state_tracker.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/base/url_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

namespace {

constexpr char kEduCoexistenceLoginURLSwitch[] = "edu-coexistence-url";
constexpr char kEduCoexistenceLoginDefaultURL[] =
    "https://families.google.com/supervision/coexistence/intro";
constexpr char kOobe[] = "oobe";
constexpr char kInSession[] = "in_session";
constexpr char kOnErrorWebUIListener[] = "show-error-screen";

constexpr char kFetchAccessTokenResultHistogram[] =
    "AccountManager.EduCoexistence.FetchAccessTokenResult";

signin::IdentityManager* GetIdentityManager() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  return IdentityManagerFactory::GetForProfile(profile);
}

std::string GetEduCoexistenceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // This should only be set during local development tests.
  if (command_line->HasSwitch(kEduCoexistenceLoginURLSwitch))
    return command_line->GetSwitchValueASCII(kEduCoexistenceLoginURLSwitch);

  return kEduCoexistenceLoginDefaultURL;
}

std::string GetSourceUI() {
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked())
    return kOobe;
  return kInSession;
}

std::string GetOrCreateEduCoexistenceUserId() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  std::string id = pref_service->GetString(prefs::kEduCoexistenceId);
  if (id.empty()) {
    id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    pref_service->SetString(prefs::kEduCoexistenceId, id);
  }
  return id;
}

base::Time GetSigninTime() {
  const Profile* profile = ProfileManager::GetActiveUserProfile();
  const PrefService* pref_service = profile->GetPrefs();
  base::Time signin_time = pref_service->GetTime(prefs::kOobeOnboardingTime);
  DCHECK(!signin_time.is_min());
  return signin_time;
}

// Tries to get the policy device id for the family link user profile if
// available. The policy device id is used to identify the device login
// timestamp to skip parental reauth for secondary edu account login during
// onboarding. If the id is not available, then the parental reauth will be
// required.
std::string GetDeviceIdForActiveUserProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  const policy::UserCloudPolicyManagerAsh* policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  if (!policy_manager)
    return std::string();

  const policy::CloudPolicyCore* core = policy_manager->core();
  const policy::CloudPolicyStore* store = core->store();
  if (!store)
    return std::string();

  const enterprise_management::PolicyData* policy = store->policy();
  if (!policy)
    return std::string();

  return policy->device_id();
}

}  // namespace

void EduCoexistenceLoginHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kEduCoexistenceId,
                               std::string() /* default_value */);
}

EduCoexistenceLoginHandler::EduCoexistenceLoginHandler(
    const base::RepeatingClosure& close_dialog_closure)
    : EduCoexistenceLoginHandler(close_dialog_closure, GetIdentityManager()) {}

EduCoexistenceLoginHandler::EduCoexistenceLoginHandler(
    const base::RepeatingClosure& close_dialog_closure,
    signin::IdentityManager* identity_manager)
    : close_dialog_closure_(close_dialog_closure),
      identity_manager_(identity_manager) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile->IsChild());

  // Start observing IdentityManager.
  identity_manager->AddObserver(this);

  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kKidsSupervisionSetupChildOAuth2Scope);
  scopes.insert(GaiaConstants::kAccountsReauthOAuth2Scope);
  scopes.insert(GaiaConstants::kAuditRecordingOAuth2Scope);
  scopes.insert(GaiaConstants::kClearCutOAuth2Scope);
  scopes.insert(GaiaConstants::kKidManagementPrivilegedOAuth2Scope);

  // Start fetching oauth access token.
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "EduCoexistenceLoginHandler", identity_manager, scopes,
          base::BindOnce(
              &EduCoexistenceLoginHandler::OnOAuthAccessTokensFetched,
              base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSync);
}

EduCoexistenceLoginHandler::~EduCoexistenceLoginHandler() {
  identity_manager_->RemoveObserver(this);

  EduCoexistenceStateTracker::Get()->OnDialogClosed(web_ui());
  close_dialog_closure_.Run();
}

void EduCoexistenceLoginHandler::RegisterMessages() {
  // Notifying |EduCoexistenceStateTracker| here instead of in the constructor
  // because the WebUI has not yet been set there.
  EduCoexistenceStateTracker::Get()->OnDialogCreated(
      web_ui(), /* is_onboarding */ session_manager::SessionManager::Get()
                    ->IsUserSessionBlocked());

  web_ui()->RegisterMessageCallback(
      "initializeEduArgs",
      base::BindRepeating(&EduCoexistenceLoginHandler::InitializeEduArgs,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "consentValid",
      base::BindRepeating(&EduCoexistenceLoginHandler::ConsentValid,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "consentLogged",
      base::BindRepeating(&EduCoexistenceLoginHandler::ConsentLogged,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "error", base::BindRepeating(&EduCoexistenceLoginHandler::OnError,
                                   base::Unretained(this)));
}

void EduCoexistenceLoginHandler::OnJavascriptDisallowed() {
  access_token_fetcher_.reset();
}

void EduCoexistenceLoginHandler::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (edu_account_email_.empty() || account_info.email != edu_account_email_)
    return;

  AllowJavascript();

  Profile* profile = ProfileManager::GetActiveUserProfile();

  edu_coexistence::UpdateAcceptedToSVersionPref(
      profile, edu_coexistence::UserConsentInfo(
                   account_info.gaia, terms_of_service_version_number_));

  EduCoexistenceStateTracker::Get()->OnWebUiStateChanged(
      web_ui(), EduCoexistenceStateTracker::FlowResult::kAccountAdded);

  // Otherwise, notify the ui that account addition was successful!!
  ResolveJavascriptCallback(base::Value(account_added_callback_),
                            base::Value(true));

  account_added_callback_.clear();
  terms_of_service_version_number_.clear();
}

void EduCoexistenceLoginHandler::OnOAuthAccessTokensFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo info) {
  base::UmaHistogramEnumeration(kFetchAccessTokenResultHistogram, error.state(),
                                GoogleServiceAuthError::NUM_STATES);

  if (error.state() != GoogleServiceAuthError::State::NONE) {
    if (initialize_edu_args_callback_.has_value()) {
      FireWebUIListener(kOnErrorWebUIListener);
    }

    EduCoexistenceStateTracker::Get()->OnWebUiStateChanged(
        web_ui(), EduCoexistenceStateTracker::FlowResult::kError);
    in_error_state_ = true;
    return;
  }

  oauth_access_token_ = info;
  if (initialize_edu_args_callback_.has_value()) {
    SendInitializeEduArgs();
  }
}

void EduCoexistenceLoginHandler::InitializeEduArgs(
    const base::Value::List& args) {
  AllowJavascript();

  initialize_edu_args_callback_ = args[0].GetString();

  if (in_error_state_) {
    FireWebUIListener(kOnErrorWebUIListener);
    return;
  }

  // If the access token is not fetched yet, wait for access token.
  if (!oauth_access_token_.has_value()) {
    return;
  }

  SendInitializeEduArgs();
}

void EduCoexistenceLoginHandler::SendInitializeEduArgs() {
  DCHECK(oauth_access_token_.has_value());
  DCHECK(initialize_edu_args_callback_.has_value());
  base::Value::Dict params;

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  params.Set("hl", app_locale);

  params.Set("url", GetEduCoexistenceURL());

  params.Set("clientId", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.Set("sourceUi", GetSourceUI());

  params.Set("clientVersion", chrome::kChromeVersion);
  params.Set("eduCoexistenceAccessToken", oauth_access_token_->token);
  params.Set("eduCoexistenceId", GetOrCreateEduCoexistenceUserId());
  params.Set("platformVersion", base::SysInfo::OperatingSystemVersion());
  // Extended stable channel is not supported on Chrome OS Ash.
  params.Set("releaseChannel",
             chrome::GetChannelName(chrome::WithExtendedStable(false)));
  params.Set("deviceId", GetDeviceIdForActiveUserProfile());

  params.Set("signinTime",
             GetSigninTime().InMillisecondsFSinceUnixEpochIgnoringNull());

  // If the secondary edu account is being reauthenticated, the email address
  // will be provided via the url of the webcontent. Example
  // chrome://chrome-signin/edu-coexistence?email=testuser1%40gmail.com
  content::WebContents* web_contents = web_ui()->GetWebContents();
  if (web_contents) {
    const GURL& current_url = web_contents->GetURL();
    std::string default_email;
    if (net::GetValueForKeyInQuery(current_url, "email", &default_email)) {
      params.Set("email", default_email);

      std::string read_only_email;
      if (net::GetValueForKeyInQuery(current_url, "readOnlyEmail",
                                     &read_only_email)) {
        params.Set("readOnlyEmail", read_only_email);
      }
    }
  }

  ResolveJavascriptCallback(base::Value(initialize_edu_args_callback_.value()),
                            params);
  initialize_edu_args_callback_ = std::nullopt;
}

void EduCoexistenceLoginHandler::ConsentValid(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!in_error_state_);
  EduCoexistenceStateTracker::Get()->OnWebUiStateChanged(
      web_ui(), EduCoexistenceStateTracker::FlowResult::kConsentValid);
}

void EduCoexistenceLoginHandler::ConsentLogged(const base::Value::List& args) {
  if (args.size() == 0)
    return;

  DCHECK(!in_error_state_);

  account_added_callback_ = args[0].GetString();

  const base::Value::List& arguments = args[1].GetList();

  edu_account_email_ = arguments[0].GetString();
  terms_of_service_version_number_ = arguments[1].GetString();

  // Notify EduCoexistenceStateTracker that consent has been logged.
  EduCoexistenceStateTracker::Get()->OnConsentLogged(web_ui(),
                                                     edu_account_email_);
}

void EduCoexistenceLoginHandler::OnError(const base::Value::List& args) {
  AllowJavascript();
  if (args.size() == 0)
    return;
  in_error_state_ = true;
  for (const base::Value& message : args) {
    DCHECK(message.is_string());
    LOG(ERROR) << message.GetString();
  }

  EduCoexistenceStateTracker::Get()->OnWebUiStateChanged(
      web_ui(), EduCoexistenceStateTracker::FlowResult::kError);
}

}  // namespace ash
