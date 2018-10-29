// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_prefs.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "components/signin/core/browser/signin_manager_base.h"
#else
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#endif

using browser_sync::ProfileSyncService;
using content::WebContents;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;

namespace {

// A structure which contains all the configuration information for sync.
struct SyncConfigInfo {
  SyncConfigInfo();
  ~SyncConfigInfo();

  bool encrypt_all;
  bool sync_everything;
  syncer::ModelTypeSet data_types;
  bool payments_integration_enabled;
  std::string passphrase;
  bool set_new_passphrase;
};

SyncConfigInfo::SyncConfigInfo()
    : encrypt_all(false),
      sync_everything(false),
      payments_integration_enabled(false),
      set_new_passphrase(false) {}

SyncConfigInfo::~SyncConfigInfo() {}

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  std::unique_ptr<base::Value> parsed_value = base::JSONReader::Read(json);
  base::DictionaryValue* result;
  if (!parsed_value || !parsed_value->GetAsDictionary(&result)) {
    DLOG(ERROR) << "GetConfiguration() not passed a Dictionary";
    return false;
  }

  if (!result->GetBoolean("syncAllDataTypes", &config->sync_everything)) {
    DLOG(ERROR) << "GetConfiguration() not passed a syncAllDataTypes value";
    return false;
  }

  if (!result->GetBoolean("paymentsIntegrationEnabled",
                          &config->payments_integration_enabled)) {
    DLOG(ERROR) << "GetConfiguration() not passed a paymentsIntegrationEnabled "
                << "value";
    return false;
  }

  syncer::ModelTypeNameMap type_names = syncer::GetUserSelectableTypeNameMap();

  for (syncer::ModelTypeNameMap::const_iterator it = type_names.begin();
       it != type_names.end(); ++it) {
    std::string key_name = it->second + std::string("Synced");
    bool sync_value;
    if (!result->GetBoolean(key_name, &sync_value)) {
      DLOG(ERROR) << "GetConfiguration() not passed a value for " << key_name;
      return false;
    }
    if (sync_value)
      config->data_types.Put(it->first);
  }

  // Encryption settings.
  if (!result->GetBoolean("encryptAllData", &config->encrypt_all)) {
    DLOG(ERROR) << "GetConfiguration() not passed a value for encryptAllData";
    return false;
  }

  // Passphrase settings.
  if (result->GetString("passphrase", &config->passphrase) &&
      !config->passphrase.empty() &&
      !result->GetBoolean("setNewPassphrase", &config->set_new_passphrase)) {
    DLOG(ERROR) << "GetConfiguration() not passed a set_new_passphrase value";
    return false;
  }
  return true;
}

// Guaranteed to return a valid result (or crash).
void ParseConfigurationArguments(const base::ListValue* args,
                                 SyncConfigInfo* config,
                                 const base::Value** callback_id) {
  std::string json;
  if (args->Get(0, callback_id) && args->GetString(1, &json) && !json.empty())
    CHECK(GetConfiguration(json, config));
  else
    NOTREACHED();
}

std::string GetSyncErrorAction(sync_ui_util::ActionType action_type) {
  switch (action_type) {
    case sync_ui_util::REAUTHENTICATE:
      return "reauthenticate";
    case sync_ui_util::SIGNOUT_AND_SIGNIN:
      return "signOutAndSignIn";
    case sync_ui_util::UPGRADE_CLIENT:
      return "upgradeClient";
    case sync_ui_util::ENTER_PASSPHRASE:
      return "enterPassphrase";
    case sync_ui_util::CONFIRM_SYNC_SETTINGS:
      return "confirmSyncSettings";
    default:
      return "noAction";
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Returns the base::Value associated with the account, to use in the stored
// accounts list.
base::Value GetAccountValue(const AccountInfo& account,
                            AccountTrackerService* account_tracker) {
  DCHECK(!account.IsEmpty());
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetKey("email", base::Value(account.email));
  dictionary.SetKey("fullName", base::Value(account.full_name));
  dictionary.SetKey("givenName", base::Value(account.given_name));
  const gfx::Image& account_image =
      account_tracker->GetAccountImage(account.account_id);
  if (!account_image.IsEmpty()) {
    dictionary.SetKey(
        "avatarImage",
        base::Value(webui::GetBitmapDataUrl(account_image.AsBitmap())));
  }
  return dictionary;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

namespace settings {

// static
const char PeopleHandler::kSpinnerPageStatus[] = "spinner";
const char PeopleHandler::kConfigurePageStatus[] = "configure";
const char PeopleHandler::kTimeoutPageStatus[] = "timeout";
const char PeopleHandler::kDonePageStatus[] = "done";
const char PeopleHandler::kPassphraseFailedPageStatus[] = "passphraseFailed";

PeopleHandler::PeopleHandler(Profile* profile)
    : profile_(profile),
      configuring_sync_(false),
      identity_manager_observer_(this),
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      sync_service_observer_(this),
      account_tracker_observer_(this) {
}
#else
      sync_service_observer_(this) {
}
#endif

PeopleHandler::~PeopleHandler() {
  // Early exit if running unit tests (no actual WebUI is attached).
  if (!web_ui())
    return;

  // Note that if the user left the sync page by closing the tab, refresh,
  // or via the back navigation, it would first go through OnDidClosePage().
  CloseSyncSetup();
}

void PeopleHandler::RegisterMessages() {
  InitializeSyncBlocker();
  web_ui()->RegisterMessageCallback(
      "SyncSetupDidClosePage",
      base::BindRepeating(&PeopleHandler::OnDidClosePage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupSetDatatypes",
      base::BindRepeating(&PeopleHandler::HandleSetDatatypes,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupSetEncryption",
      base::BindRepeating(&PeopleHandler::HandleSetEncryption,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupShowSetupUI",
      base::BindRepeating(&PeopleHandler::HandleShowSetupUI,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupGetSyncStatus",
      base::BindRepeating(&PeopleHandler::HandleGetSyncStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupManageOtherPeople",
      base::BindRepeating(&PeopleHandler::HandleManageOtherPeople,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "UnifiedConsentToggleChanged",
      base::BindRepeating(&PeopleHandler::OnUnifiedConsentToggleChanged,
                          base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "AttemptUserExit",
      base::BindRepeating(&PeopleHandler::HandleAttemptUserExit,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "RequestPinLoginState",
      base::BindRepeating(&PeopleHandler::HandleRequestPinLoginState,
                          base::Unretained(this)));
#else
  web_ui()->RegisterMessageCallback(
      "SyncSetupSignout", base::BindRepeating(&PeopleHandler::HandleSignout,
                                              base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupPauseSync", base::BindRepeating(&PeopleHandler::HandlePauseSync,
                                                base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupStartSignIn",
      base::BindRepeating(&PeopleHandler::HandleStartSignin,
                          base::Unretained(this)));
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  web_ui()->RegisterMessageCallback(
      "SyncSetupGetStoredAccounts",
      base::BindRepeating(&PeopleHandler::HandleGetStoredAccounts,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupStartSyncingWithEmail",
      base::BindRepeating(&PeopleHandler::HandleStartSyncingWithEmail,
                          base::Unretained(this)));
#endif
}

void PeopleHandler::OnJavascriptAllowed() {
  PrefService* prefs = profile_->GetPrefs();
  profile_pref_registrar_.Init(prefs);
  profile_pref_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&PeopleHandler::UpdateSyncStatus, base::Unretained(this)));

  identity::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager)
    identity_manager_observer_.Add(identity_manager);

  // This is intentionally not using GetSyncService(), to go around the
  // Profile::IsSyncAllowed() check.
  ProfileSyncService* sync_service(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_));
  if (sync_service)
    sync_service_observer_.Add(sync_service);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  AccountTrackerService* account_tracker(
      AccountTrackerServiceFactory::GetForProfile(profile_));
  if (account_tracker)
    account_tracker_observer_.Add(account_tracker);
#endif
}

void PeopleHandler::OnJavascriptDisallowed() {
  profile_pref_registrar_.RemoveAll();
  identity_manager_observer_.RemoveAll();
  sync_service_observer_.RemoveAll();
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  account_tracker_observer_.RemoveAll();
#endif
}

#if !defined(OS_CHROMEOS)
void PeopleHandler::DisplayGaiaLogin(signin_metrics::AccessPoint access_point) {
  DCHECK(!sync_startup_tracker_);
  // Advanced options are no longer being configured if the login screen is
  // visible. If the user exits the signin wizard after this without
  // configuring sync, CloseSyncSetup() will ensure they are logged out.
  configuring_sync_ = false;
  DisplayGaiaLoginInNewTabOrWindow(access_point);
}

void PeopleHandler::DisplayGaiaLoginInNewTabOrWindow(
    signin_metrics::AccessPoint access_point) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  bool force_new_tab = false;
  if (!browser) {
    // Settings is not displayed in a browser window. Open a new window.
    browser = new Browser(
        Browser::CreateParams(Browser::TYPE_TABBED, profile_, true));
    force_new_tab = true;
  }

  ProfileSyncService* service = GetSyncService();
  if (service && service->HasUnrecoverableError()) {
    // When the user has an unrecoverable error, they first have to sign out and
    // then sign in again.
    SigninManagerFactory::GetForProfile(browser->profile())
        ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                  signin_metrics::SignoutDelete::IGNORE_METRIC);
  }

  // If the signin manager already has an authenticated username, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  GURL url;
  if (SigninManagerFactory::GetForProfile(browser->profile())
          ->IsAuthenticated()) {
    UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                              signin_metrics::HISTOGRAM_REAUTH_SHOWN,
                              signin_metrics::HISTOGRAM_REAUTH_MAX);

    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(browser->profile());
    DCHECK(error_controller->HasError());
    if (!force_new_tab) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH,
          signin::ManageAccountsParams(), access_point, false);
    } else {
      url = signin::GetReauthURLForTab(
          access_point, signin_metrics::Reason::REASON_REAUTHENTICATION,
          browser->profile(), error_controller->error_account_id());
    }
  } else {
    if (!force_new_tab) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,
          signin::ManageAccountsParams(), access_point, false);
    } else {
      url = signin::GetPromoURLForTab(
          access_point, signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
          true);
    }
  }

  if (url.is_valid())
    ShowSingletonTab(browser, url);
}
#endif

#if defined(OS_CHROMEOS)
void PeopleHandler::OnPinLoginAvailable(bool is_available) {
  FireWebUIListener("pin-login-available-changed", base::Value(is_available));
}
#endif

void PeopleHandler::DisplaySpinner() {
  configuring_sync_ = true;

  const int kTimeoutSec = 30;
  DCHECK(!engine_start_timer_);
  engine_start_timer_.reset(new base::OneShotTimer());
  engine_start_timer_->Start(FROM_HERE,
                             base::TimeDelta::FromSeconds(kTimeoutSec), this,
                             &PeopleHandler::DisplayTimeout);

  FireWebUIListener("page-status-changed", base::Value(kSpinnerPageStatus));
}

void PeopleHandler::DisplayTimeout() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  // Do not listen to sync startup events.
  sync_startup_tracker_.reset();

  FireWebUIListener("page-status-changed", base::Value(kTimeoutPageStatus));
}

void PeopleHandler::OnDidClosePage(const base::ListValue* args) {
  // Don't mark setup as complete if "didAbort" is true, or if authentication
  // is still needed.
  if (!args->GetList()[0].GetBool() && !IsProfileAuthNeededOrHasErrors()) {
    MarkFirstSetupComplete();
  }

  CloseSyncSetup();
}

void PeopleHandler::SyncStartupFailed() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  // Just close the sync overlay (the idea is that the base settings page will
  // display the current error.)
  CloseUI();
}

void PeopleHandler::SyncStartupCompleted() {
  ProfileSyncService* service = GetSyncService();
  DCHECK(service->IsEngineInitialized());

  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  sync_startup_tracker_.reset();

  PushSyncPrefs();
}

ProfileSyncService* PeopleHandler::GetSyncService() const {
  return profile_->IsSyncAllowed()
             ? ProfileSyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void PeopleHandler::HandleSetDatatypes(const base::ListValue* args) {
  DCHECK(!sync_startup_tracker_);

  SyncConfigInfo configuration;
  const base::Value* callback_id = nullptr;
  ParseConfigurationArguments(args, &configuration, &callback_id);

  autofill::prefs::SetPaymentsIntegrationEnabled(
      profile_->GetPrefs(), configuration.payments_integration_enabled);

  // Start configuring the ProfileSyncService using the configuration passed
  // to us from the JS layer.
  ProfileSyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just close the sync
  // dialog.
  if (!service || !service->IsEngineInitialized()) {
    CloseSyncSetup();
    ResolveJavascriptCallback(*callback_id, base::Value(kDonePageStatus));
    return;
  }

  service->OnUserChoseDatatypes(configuration.sync_everything,
                                configuration.data_types);

  // Choosing data types to sync never fails.
  ResolveJavascriptCallback(*callback_id, base::Value(kConfigurePageStatus));

  ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CUSTOMIZE);
  if (!configuration.sync_everything)
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CHOOSE);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void PeopleHandler::HandleGetStoredAccounts(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id, *GetStoredAccountsList());
}

void PeopleHandler::OnAccountUpdated(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-updated", *GetStoredAccountsList());
}

void PeopleHandler::OnAccountImageUpdated(const std::string& account_id,
                                          const gfx::Image& image) {
  FireWebUIListener("stored-accounts-updated", *GetStoredAccountsList());
}

void PeopleHandler::OnAccountRemoved(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-updated", *GetStoredAccountsList());
}

std::unique_ptr<base::ListValue> PeopleHandler::GetStoredAccountsList() {
  std::unique_ptr<base::ListValue> accounts_list =
      std::make_unique<base::ListValue>();
  bool dice_enabled =
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_);

  // Dice and unified consent both disabled: do not show the list of accounts.
  if (!dice_enabled && !unified_consent::IsUnifiedConsentFeatureEnabled())
    return accounts_list;

  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);

  if (dice_enabled) {
    // If dice is enabled, show all the accounts.
    std::vector<AccountInfo> accounts =
        signin_ui_util::GetAccountsForDicePromos(profile_);
    accounts_list->Reserve(accounts.size());
    for (auto const& account : accounts) {
      accounts_list->GetList().push_back(
          GetAccountValue(account, account_tracker));
    }
  } else {
    // If dice is disabled (and unified consent enabled), show only the primary
    // account.
    std::string primary_account = SigninManagerFactory::GetForProfile(profile_)
                                      ->GetAuthenticatedAccountId();
    if (!primary_account.empty()) {
      accounts_list->GetList().push_back(GetAccountValue(
          account_tracker->GetAccountInfo(primary_account), account_tracker));
    }
  }

  return accounts_list;
}

void PeopleHandler::HandleStartSyncingWithEmail(const base::ListValue* args) {
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_));
  const base::Value* email;
  const base::Value* is_default_promo_account;
  CHECK(args->Get(0, &email));
  CHECK(args->Get(1, &is_default_promo_account));

  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());

  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  AccountInfo account =
      account_tracker->FindAccountInfoByEmail(email->GetString());
  signin_ui_util::EnableSyncFromPromo(
      browser, account, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
      is_default_promo_account->GetBool());
}
#endif

void PeopleHandler::HandleSetEncryption(const base::ListValue* args) {
  DCHECK(!sync_startup_tracker_);

  SyncConfigInfo configuration;
  const base::Value* callback_id = nullptr;
  ParseConfigurationArguments(args, &configuration, &callback_id);

  // Start configuring the ProfileSyncService using the configuration passed
  // to us from the JS layer.
  ProfileSyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just close the sync
  // dialog.
  if (!service || !service->IsEngineInitialized()) {
    CloseSyncSetup();
    ResolveJavascriptCallback(*callback_id, base::Value(kDonePageStatus));
    return;
  }

  // Don't allow "encrypt all" if the ProfileSyncService doesn't allow it.
  // The UI is hidden, but the user may have enabled it e.g. by fiddling with
  // the web inspector.
  if (!service->IsEncryptEverythingAllowed())
    configuration.encrypt_all = false;

  // Note: Data encryption will not occur until configuration is complete
  // (when the PSS receives its CONFIGURE_DONE notification from the sync
  // engine), so the user still has a chance to cancel out of the operation
  // if (for example) some kind of passphrase error is encountered.
  if (configuration.encrypt_all)
    service->EnableEncryptEverything();

  bool passphrase_failed = false;
  if (!configuration.passphrase.empty()) {
    // We call IsPassphraseRequired() here (instead of
    // IsPassphraseRequiredForDecryption()) because the user may try to enter
    // a passphrase even though no encrypted data types are enabled.
    if (service->IsPassphraseRequired()) {
      // If we have pending keys, try to decrypt them with the provided
      // passphrase. We track if this succeeds or fails because a failed
      // decryption should result in an error even if there aren't any encrypted
      // data types.
      passphrase_failed =
          !service->SetDecryptionPassphrase(configuration.passphrase);
    } else {
      // OK, the user sent us a passphrase, but we don't have pending keys. So
      // it either means that the pending keys were resolved somehow since the
      // time the UI was displayed (re-encryption, pending passphrase change,
      // etc) or the user wants to re-encrypt.
      if (configuration.set_new_passphrase &&
          !service->IsUsingSecondaryPassphrase()) {
        service->SetEncryptionPassphrase(configuration.passphrase);
      }
    }
  }

  if (passphrase_failed || service->IsPassphraseRequiredForDecryption()) {
    // If the user doesn't enter any passphrase, we won't call
    // SetDecryptionPassphrase() (passphrase_failed == false), but we still
    // want to display an error message to let the user know that their blank
    // passphrase entry is not acceptable.

    // TODO(tommycli): Switch this to RejectJavascriptCallback once the
    // Sync page JavaScript has been further refactored.
    ResolveJavascriptCallback(*callback_id,
                              base::Value(kPassphraseFailedPageStatus));
  } else {
    ResolveJavascriptCallback(*callback_id, base::Value(kConfigurePageStatus));
  }

  if (configuration.encrypt_all)
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_ENCRYPT);
  if (!configuration.set_new_passphrase && !configuration.passphrase.empty())
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_PASSPHRASE);
}

void PeopleHandler::HandleShowSetupUI(const base::ListValue* args) {
  AllowJavascript();

  ProfileSyncService* service = GetSyncService();

  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    if (service && !sync_blocker_)
      sync_blocker_ = service->GetSetupInProgressHandle();

    GetLoginUIService()->SetLoginUI(this);

    PushSyncPrefs();
    // Always let the page open when unified consent is enabled.
    return;
  }

  if (!service) {
    CloseUI();
    return;
  }

  // This if-statement is not using IsProfileAuthNeededOrHasErrors(), because
  // in some error cases (e.g. "confirmSyncSettings") the UI still needs to
  // show.
  if (!SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated()) {
    // For web-based signin, the signin page is not displayed in an overlay
    // on the settings page. So if we get here, it must be due to the user
    // cancelling signin (by reloading the sync settings page during initial
    // signin) or by directly navigating to settings/syncSetup
    // (http://crbug.com/229836). So just exit and go back to the settings page.
    DLOG(WARNING) << "Cannot display sync setup UI when not signed in";
    CloseUI();
    return;
  }

  // Notify services that login UI is now active.
  GetLoginUIService()->SetLoginUI(this);

  if (!sync_blocker_)
    sync_blocker_ = service->GetSetupInProgressHandle();

  // Early exit if there is already a preferences push pending sync startup.
  if (sync_startup_tracker_)
    return;

  if (!service->IsEngineInitialized() ||
      service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE)) {
    // Requesting the sync service to start may trigger call to PushSyncPrefs.
    // Setting up the startup tracker beforehand correctly signals the
    // re-entrant call to early exit.
    sync_startup_tracker_ =
        std::make_unique<SyncStartupTracker>(profile_, this);
    // RequestStart() does two things:
    // 1) If DISABLE_REASON_USER_CHOICE is set (meaning that Sync was reset via
    //    the dashboard), clears it.
    // 2) Pokes the sync service to start *immediately*, i.e. bypass deferred
    //    startup.
    // It's possible that both of these are already the case, i.e. the engine is
    // already in the process of initializing, in which case RequestStart() will
    // effectively do nothing. It's also possible that the sync service is
    // already running in standalone transport mode and so the engine is already
    // initialized. In that case, this will trigger the service to switch to
    // full Sync-the-feature mode.
    service->RequestStart();

    // See if it's even possible to bring up the sync engine - if not
    // (unrecoverable error?), don't bother displaying a spinner that will be
    // immediately closed because this leads to some ugly infinite UI loop (see
    // http://crbug.com/244769).
    if (SyncStartupTracker::GetSyncServiceState(profile_) !=
        SyncStartupTracker::SYNC_STARTUP_ERROR) {
      DisplaySpinner();
    }
    return;
  }

  // User is already logged in. They must have brought up the config wizard
  // via the "Advanced..." button or through One-Click signin (cases 4-6), or
  // they are re-enabling sync after having disabled it (case 7).
  PushSyncPrefs();
}

#if defined(OS_CHROMEOS)
// On ChromeOS, we need to sign out the user session to fix an auth error, so
// the user goes through the real signin flow to generate a new auth token.
void PeopleHandler::HandleAttemptUserExit(const base::ListValue* args) {
  DVLOG(1) << "Signing out the user to fix a sync error.";
  chrome::AttemptUserExit();
}

void PeopleHandler::HandleRequestPinLoginState(const base::ListValue* args) {
  AllowJavascript();
  chromeos::quick_unlock::PinBackend::GetInstance()->HasLoginSupport(
      base::BindOnce(&PeopleHandler::OnPinLoginAvailable,
                     weak_factory_.GetWeakPtr()));
}
#endif

#if !defined(OS_CHROMEOS)
void PeopleHandler::HandleStartSignin(const base::ListValue* args) {
  AllowJavascript();

  // Should only be called if the user is not already signed in, has a auth
  // error, or a unrecoverable sync error requiring re-auth.
  ProfileSyncService* service = GetSyncService();
  DCHECK(IsProfileAuthNeededOrHasErrors() ||
         (service && service->HasUnrecoverableError()));

  DisplayGaiaLogin(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
}

void PeopleHandler::HandleSignout(const base::ListValue* args) {
  bool delete_profile = false;
  args->GetBoolean(0, &delete_profile);

  if (!signin_util::IsUserSignoutAllowedForProfile(profile_)) {
    // If the user cannot signout, the profile must be destroyed.
    DCHECK(delete_profile);
  } else {
    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfile(profile_);
    if (signin_manager->IsAuthenticated()) {
      if (GetSyncService())
        ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);

      signin_metrics::SignoutDelete delete_metric =
          delete_profile ? signin_metrics::SignoutDelete::DELETED
                         : signin_metrics::SignoutDelete::KEEPING;
      signin_manager->SignOutAndRemoveAllAccounts(
          signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, delete_metric);
    } else {
      DCHECK(!delete_profile)
          << "Deleting the profile should only be offered the user is syncing.";
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
          ->RevokeAllCredentials();
    }
  }

  if (delete_profile) {
    webui::DeleteProfileAtPath(profile_->GetPath(),
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }
}

void PeopleHandler::HandlePauseSync(const base::ListValue* args) {
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_));
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  DCHECK(signin_manager->IsAuthenticated());
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->UpdateCredentials(
      signin_manager->GetAuthenticatedAccountId(),
      OAuth2TokenServiceDelegate::kInvalidRefreshToken);
}
#endif

void PeopleHandler::HandleGetSyncStatus(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id, *GetSyncStatusDictionary());
}

void PeopleHandler::HandleManageOtherPeople(const base::ListValue* /* args */) {
#if !defined(OS_CHROMEOS)
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
#endif  // !defined(OS_CHROMEOS)
}

void PeopleHandler::OnUnifiedConsentToggleChanged(const base::ListValue* args) {
  bool is_toggle_checked = args->GetList()[0].GetBool();
  if (!is_toggle_checked) {
    unified_consent::metrics::RecordUnifiedConsentRevoked(
        unified_consent::metrics::UnifiedConsentRevokeReason::
            kUserDisabledSettingsToggle);
  }
}

void PeopleHandler::CloseSyncSetup() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  // Clear the sync startup tracker, since the setup wizard is being closed.
  sync_startup_tracker_.reset();

  ProfileSyncService* sync_service = GetSyncService();

  // LoginUIService can be nullptr if page is brought up in incognito mode
  // (i.e. if the user is running in guest mode in cros and brings up settings).
  LoginUIService* service = GetLoginUIService();
  if (service) {
    // Don't log a cancel event if the sync setup dialog is being
    // automatically closed due to an auth error.
    if ((service->current_login_ui() == this) &&
        (!sync_service || (!sync_service->IsFirstSetupComplete() &&
                           sync_service->GetAuthError().state() ==
                               GoogleServiceAuthError::NONE))) {
      if (configuring_sync_) {
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_DURING_CONFIGURE);

        // If the user clicked "Cancel" while setting up sync, disable sync
        // because we don't want the sync engine to remain in the
        // first-setup-incomplete state.
        // Note: In order to disable sync across restarts on Chrome OS,
        // we must call RequestStop(CLEAR_DATA), which suppresses sync startup
        // in addition to disabling it.
        if (sync_service) {
          DVLOG(1) << "Sync setup aborted by user action";
          sync_service->RequestStop(ProfileSyncService::CLEAR_DATA);
#if !defined(OS_CHROMEOS)
          // Sign out the user on desktop Chrome if they click cancel during
          // initial setup.
          if (sync_service->IsFirstSetupInProgress()) {
            SigninManagerFactory::GetForProfile(profile_)
                ->SignOut(signin_metrics::ABORT_SIGNIN,
                          signin_metrics::SignoutDelete::IGNORE_METRIC);
          }
#endif
        }
      }
    }

    service->LoginUIClosed(this);
  }

  // Alert the sync service anytime the sync setup dialog is closed. This can
  // happen due to the user clicking the OK or Cancel button, or due to the
  // dialog being closed by virtue of sync being disabled in the background.
  sync_blocker_.reset();

  configuring_sync_ = false;
}

void PeopleHandler::InitializeSyncBlocker() {
  if (!web_ui())
    return;
  WebContents* web_contents = web_ui()->GetWebContents();
  if (web_contents) {
    ProfileSyncService* service = GetSyncService();
    const GURL current_url = web_contents->GetVisibleURL();
    if (service &&
        current_url == chrome::GetSettingsUrl(chrome::kSyncSetupSubPage)) {
      sync_blocker_ = service->GetSetupInProgressHandle();
    }
  }
}

void PeopleHandler::FocusUI() {
  WebContents* web_contents = web_ui()->GetWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void PeopleHandler::CloseUI() {
  CloseSyncSetup();
  FireWebUIListener("page-status-changed", base::Value(kDonePageStatus));
}

void PeopleHandler::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  UpdateSyncStatus();
}

void PeopleHandler::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  UpdateSyncStatus();
}

void PeopleHandler::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncStatus();

  // When the SyncService changes its state, we should also push the updated
  // sync preferences.
  PushSyncPrefs();
}

std::unique_ptr<base::DictionaryValue>
PeopleHandler::GetSyncStatusDictionary() {
  std::unique_ptr<base::DictionaryValue> sync_status(new base::DictionaryValue);
  if (profile_->IsGuestSession()) {
    // Cannot display signin status when running in guest mode on chromeos
    // because there is no SigninManager.
    sync_status->SetBoolean("signinAllowed", false);
    return sync_status;
  }

  sync_status->SetBoolean("supervisedUser", profile_->IsSupervised());
  sync_status->SetBoolean("childUser", profile_->IsChild());

  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile_);
  DCHECK(signin);
#if !defined(OS_CHROMEOS)
  // Signout is not allowed if the user has policy (crbug.com/172204).
  if (!signin_util::IsUserSignoutAllowedForProfile(profile_)) {
    std::string username = signin->GetAuthenticatedAccountInfo().email;

    // If there is no one logged in or if the profile name is empty then the
    // domain name is empty. This happens in browser tests.
    if (!username.empty())
      sync_status->SetString("domain", gaia::ExtractDomainName(username));
  }
#endif

  // This is intentionally not using GetSyncService(), in order to access more
  // nuanced information, since GetSyncService() returns nullptr if anything
  // makes Profile::IsSyncAllowed() false.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  bool disallowed_by_policy =
      service && service->HasDisableReason(
                     syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  sync_status->SetBoolean("signinAllowed", signin->IsSigninAllowed());
  sync_status->SetBoolean("syncSystemEnabled", (service != nullptr));
  sync_status->SetBoolean("setupInProgress",
                          service && !disallowed_by_policy &&
                              service->IsFirstSetupInProgress() &&
                              signin->IsAuthenticated());

  base::string16 status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  bool status_has_error =
      sync_ui_util::GetStatusLabels(profile_, service, *signin, &status_label,
                                    &link_label,
                                    &action_type) == sync_ui_util::SYNC_ERROR;
  sync_status->SetString("statusText", status_label);
  sync_status->SetBoolean("hasError", status_has_error);
  sync_status->SetString("statusAction", GetSyncErrorAction(action_type));

  sync_status->SetBoolean("managed", disallowed_by_policy);
  sync_status->SetBoolean(
      "disabled",
      !service || disallowed_by_policy ||
          service->HasDisableReason(
              syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE));
  sync_status->SetBoolean("signedIn", signin->IsAuthenticated());
  sync_status->SetString("signedInUsername",
                         signin_ui_util::GetAuthenticatedUsername(signin));
  sync_status->SetBoolean("hasUnrecoverableError",
                          service && service->HasUnrecoverableError());
  return sync_status;
}

void PeopleHandler::PushSyncPrefs() {
#if !defined(OS_CHROMEOS)
  // Early exit if the user has not signed in yet.
  if (IsProfileAuthNeededOrHasErrors())
    return;
#endif

  ProfileSyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service || !service->IsEngineInitialized()) {
    return;
  }

  configuring_sync_ = true;
  DCHECK(service->IsEngineInitialized())
      << "Cannot configure sync until the sync engine is initialized";

  // Setup args for the sync configure screen:
  //   syncAllDataTypes: true if the user wants to sync everything
  //   <data_type>Registered: true if the associated data type is supported
  //   <data_type>Synced: true if the user wants to sync that specific data type
  //   paymentsIntegrationEnabled: true if the user wants Payments integration
  //   encryptionEnabled: true if sync supports encryption
  //   encryptAllData: true if user wants to encrypt all data (not just
  //       passwords)
  //   passphraseRequired: true if a passphrase is needed to start sync
  //   passphraseTypeIsCustom: true if the passphrase type is custom
  //
  base::DictionaryValue args;

  // Tell the UI layer which data types are registered/enabled by the user.
  const syncer::ModelTypeSet registered_types =
      service->GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types = service->GetPreferredDataTypes();
  const syncer::ModelTypeSet enforced_types = service->GetForcedDataTypes();
  syncer::ModelTypeNameMap type_names = syncer::GetUserSelectableTypeNameMap();
  for (syncer::ModelTypeNameMap::const_iterator it = type_names.begin();
       it != type_names.end(); ++it) {
    syncer::ModelType sync_type = it->first;
    const std::string key_name = it->second;
    args.SetBoolean(key_name + "Registered", registered_types.Has(sync_type));
    args.SetBoolean(key_name + "Synced", preferred_types.Has(sync_type));
    args.SetBoolean(key_name + "Enforced", enforced_types.Has(sync_type));
    // TODO(treib): How do we want to handle pref groups, i.e. when only some of
    // the sync types behind a checkbox are force-enabled? crbug.com/403326
  }
  PrefService* pref_service = profile_->GetPrefs();
  syncer::SyncPrefs sync_prefs(pref_service);
  args.SetBoolean("syncAllDataTypes", sync_prefs.HasKeepEverythingSynced());
  args.SetBoolean("paymentsIntegrationEnabled",
                  autofill::prefs::IsPaymentsIntegrationEnabled(pref_service));
  args.SetBoolean("encryptAllData", service->IsEncryptEverythingEnabled());
  args.SetBoolean("encryptAllDataAllowed",
                  service->IsEncryptEverythingAllowed());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForDecryption(), because we want to show the passphrase
  // UI even if no encrypted data types are enabled.
  args.SetBoolean("passphraseRequired", service->IsPassphraseRequired());

  // To distinguish between PassphraseType::FROZEN_IMPLICIT_PASSPHRASE and
  // PassphraseType::CUSTOM_PASSPHRASE
  // we only set passphraseTypeIsCustom for PassphraseType::CUSTOM_PASSPHRASE.
  args.SetBoolean("passphraseTypeIsCustom",
                  service->GetPassphraseType() ==
                      syncer::PassphraseType::CUSTOM_PASSPHRASE);
  base::Time passphrase_time = service->GetExplicitPassphraseTime();
  syncer::PassphraseType passphrase_type = service->GetPassphraseType();
  if (!passphrase_time.is_null()) {
    base::string16 passphrase_time_str =
        base::TimeFormatShortDate(passphrase_time);
    args.SetString("enterPassphraseBody",
                   GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
                                   passphrase_time_str));
    args.SetString(
        "enterGooglePassphraseBody",
        GetStringFUTF16(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
                        passphrase_time_str));
    switch (passphrase_type) {
      case syncer::PassphraseType::FROZEN_IMPLICIT_PASSPHRASE:
        args.SetString(
            "fullEncryptionBody",
            GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_GOOGLE_WITH_DATE,
                            passphrase_time_str));
        break;
      case syncer::PassphraseType::CUSTOM_PASSPHRASE:
        args.SetString(
            "fullEncryptionBody",
            GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM_WITH_DATE,
                            passphrase_time_str));
        break;
      default:
        args.SetString("fullEncryptionBody",
                       GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
        break;
    }
  } else if (passphrase_type == syncer::PassphraseType::CUSTOM_PASSPHRASE) {
    args.SetString("fullEncryptionBody",
                   GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
  }

  FireWebUIListener("sync-prefs-changed", args);
}

LoginUIService* PeopleHandler::GetLoginUIService() const {
  return LoginUIServiceFactory::GetForProfile(profile_);
}

void PeopleHandler::UpdateSyncStatus() {
  FireWebUIListener("sync-status-changed", *GetSyncStatusDictionary());
}

void PeopleHandler::MarkFirstSetupComplete() {
  // Suppress the sign in promo once the user starts sync. This way the user
  // doesn't see the sign in promo even if they sign out later on.
  signin::SetUserSkippedPromo(profile_);

  ProfileSyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service || service->IsFirstSetupComplete())
    return;

  // This is the first time configuring sync, so log it.
  base::FilePath profile_file_path = profile_->GetPath();
  ProfileMetrics::LogProfileSyncSignIn(profile_file_path);

  // We're done configuring, so notify ProfileSyncService that it is OK to
  // start syncing.
  sync_blocker_.reset();
  service->SetFirstSetupComplete();
  FireWebUIListener("sync-settings-saved");
}

bool PeopleHandler::IsProfileAuthNeededOrHasErrors() {
  return !SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated() ||
         SigninErrorControllerFactory::GetForProfile(profile_)->HasError();
}

}  // namespace settings
