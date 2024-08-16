// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/trusted_vault/features.h"
#endif

using content::WebContents;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using signin::ConsentLevel;

namespace {

const char kTrustedVaultBannerStateChangedEvent[] =
    "trusted-vault-banner-state-changed";

// WARNING: Keep synced with
// chrome/browser/resources/settings/people_page/sync_browser_proxy.ts.
enum class TrustedVaultBannerState {
  kNotShown = 0,
  kOfferOptIn = 1,
  kOptedIn = 2,
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// This enum is used for metrics purposes only, it is aligned with
// `ChromeSigninUserChoice` enum, with the exception of the
// `ChromeSigninUserChoice::kNoChoice` which is not a valid modification value.
// It is replaced here with `kNoModification`, stating that the user saw the
// setting and did not perform any modification while the setting page was
// opened and the setting was shown at some point.
//
// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class ChromeSigninSettingModification {
  kNoModification = 0,
  kToAlwaysAsk = 1,
  kToSignin = 2,
  kToDoNotSignin = 3,

  kMaxValue = kToDoNotSignin,
};
#endif

// A structure which contains all the configuration information for sync.
struct SyncConfigInfo {
  SyncConfigInfo();
  ~SyncConfigInfo();

  bool sync_everything;
  syncer::UserSelectableTypeSet selected_types;
};

bool IsSyncSubpage(const GURL& current_url) {
  return current_url == chrome::GetSettingsUrl(chrome::kSyncSetupSubPage);
}

SyncConfigInfo::SyncConfigInfo() : sync_everything(false) {}

SyncConfigInfo::~SyncConfigInfo() {}

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  std::optional<base::Value> parsed_value = base::JSONReader::Read(json);
  if (!parsed_value.has_value() || !parsed_value->is_dict()) {
    DLOG(ERROR) << "GetConfiguration() not passed a Dictionary";
    return false;
  }

  const base::Value::Dict& root = parsed_value->GetDict();
  std::optional<bool> sync_everything = root.FindBool("syncAllDataTypes");
  if (!sync_everything.has_value()) {
    DLOG(ERROR) << "GetConfiguration() not passed a syncAllDataTypes value";
    return false;
  }
  config->sync_everything = *sync_everything;

  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    std::string key_name =
        syncer::GetUserSelectableTypeName(type) + std::string("Synced");
    std::optional<bool> type_synced = root.FindBool(key_name);
    if (!type_synced.has_value()) {
      DLOG(ERROR) << "GetConfiguration() not passed a value for " << key_name;
      return false;
    }
    if (*type_synced) {
      config->selected_types.Put(type);
    }
  }

  return true;
}

// Guaranteed to return a valid result (or crash).
void ParseConfigurationArguments(const base::Value::List& args,
                                 SyncConfigInfo* config,
                                 const base::Value** callback_id) {
  const std::string& json = args[1].GetString();
  if ((*callback_id = &args[0]) && !json.empty()) {
    CHECK(GetConfiguration(json, config));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

std::string GetSyncErrorAction(SyncStatusActionType action_type) {
  switch (action_type) {
    case SyncStatusActionType::kReauthenticate:
      return "reauthenticate";
    case SyncStatusActionType::kUpgradeClient:
      return "upgradeClient";
    case SyncStatusActionType::kEnterPassphrase:
      return "enterPassphrase";
    case SyncStatusActionType::kRetrieveTrustedVaultKeys:
      return "retrieveTrustedVaultKeys";
    case SyncStatusActionType::kConfirmSyncSettings:
      return "confirmSyncSettings";
    case SyncStatusActionType::kNoAction:
      return "noAction";
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// Returns the base::Value associated with the account, to use in the stored
// accounts list.
base::Value::Dict GetAccountValue(signin::IdentityManager* identity_manager,
                                  const AccountInfo& account) {
  DCHECK(!account.IsEmpty());
  auto dict =
      base::Value::Dict()
          .Set("email", account.email)
          .Set("fullName", account.full_name)
          .Set("givenName", account.given_name)
          .Set("isPrimaryAccount",
               account.account_id ==
                   identity_manager
                       ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                       .account_id);
  if (!account.account_image.IsEmpty()) {
    dict.Set("avatarImage",
             webui::GetBitmapDataUrl(account.account_image.AsBitmap()));
  }
  return dict;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
bool IsChangePrimaryAccountAllowed(Profile* profile, const std::string& email) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (ChromeSigninClientFactory::GetForProfile(profile)
          ->IsClearPrimaryAccountAllowed(
              identity_manager->HasPrimaryAccount(ConsentLevel::kSync)) ||
      !identity_manager->HasPrimaryAccount(ConsentLevel::kSignin)) {
    return true;
  }

  return gaia::AreEmailsSame(
      email,
      identity_manager->GetPrimaryAccountInfo(ConsentLevel::kSignin).email);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
ChromeSigninSettingModification ChromeSigninUserChoiceToModification(
    ChromeSigninUserChoice choice) {
  switch (choice) {
    case ChromeSigninUserChoice::kAlwaysAsk:
      return ChromeSigninSettingModification::kToAlwaysAsk;
    case ChromeSigninUserChoice::kSignin:
      return ChromeSigninSettingModification::kToSignin;
    case ChromeSigninUserChoice::kDoNotSignin:
      return ChromeSigninSettingModification::kToDoNotSignin;
    case ChromeSigninUserChoice::kNoChoice:
      NOTREACHED() << "No choice is not expected as a modification";
  }
}
#endif

}  // namespace

namespace settings {

// static
const char PeopleHandler::kConfigurePageStatus[] = "configure";
const char PeopleHandler::kDonePageStatus[] = "done";
const char PeopleHandler::kPassphraseFailedPageStatus[] = "passphraseFailed";

// TODO(crbug.com/40258836): Delete parts needed only by PasswordManager once
// kPasswordManagerRedesign is launched.
PeopleHandler::PeopleHandler(Profile* profile)
    : profile_(profile), configuring_sync_(false) {}

PeopleHandler::~PeopleHandler() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (chrome_signin_user_choice_shown_ &&
      !chrome_signin_user_choice_modified_) {
    base::UmaHistogramEnumeration(
        "Signin.Settings.ChromeSigninSettingModification",
        ChromeSigninSettingModification::kNoModification);
  }
#endif

  // Early exit if running unit tests (no actual WebUI is attached).
  if (!web_ui()) {
    return;
  }

  // Remove this class as an observer to prevent calls back into this class
  // while destroying.
  OnJavascriptDisallowed();

  // If unified consent is enabled and the user left the sync page by closing
  // the tab, refresh, or via the back navigation, the sync setup needs to be
  // closed. If this was the first time setup, sync will be cancelled.
  // Note, if unified consent is disabled, it will first go through
  // |OnDidClosePage()|.
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
      "SyncSetupSetEncryptionPassphrase",
      base::BindRepeating(&PeopleHandler::HandleSetEncryptionPassphrase,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupSetDecryptionPassphrase",
      base::BindRepeating(&PeopleHandler::HandleSetDecryptionPassphrase,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupShowSetupUI",
      base::BindRepeating(&PeopleHandler::HandleShowSyncSetupUI,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupGetSyncStatus",
      base::BindRepeating(&PeopleHandler::HandleGetSyncStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncPrefsDispatch",
      base::BindRepeating(&PeopleHandler::HandleSyncPrefsDispatch,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncTrustedVaultBannerStateDispatch",
      base::BindRepeating(&PeopleHandler::HandleTrustedVaultBannerStateDispatch,
                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "AttemptUserExit",
      base::BindRepeating(&PeopleHandler::HandleAttemptUserExit,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "TurnOnSync", base::BindRepeating(&PeopleHandler::HandleTurnOnSync,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "TurnOffSync", base::BindRepeating(&PeopleHandler::HandleTurnOffSync,
                                         base::Unretained(this)));
#else
  web_ui()->RegisterMessageCallback(
      "SyncSetupStartSignIn",
      base::BindRepeating(&PeopleHandler::HandleStartSignin,
                          base::Unretained(this)));
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  web_ui()->RegisterMessageCallback(
      "SyncSetupSignout", base::BindRepeating(&PeopleHandler::HandleSignout,
                                              base::Unretained(this)));
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  web_ui()->RegisterMessageCallback(
      "SyncSetupPauseSync", base::BindRepeating(&PeopleHandler::HandlePauseSync,
                                                base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "SyncSetupGetStoredAccounts",
      base::BindRepeating(&PeopleHandler::HandleGetStoredAccounts,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupStartSyncingWithEmail",
      base::BindRepeating(&PeopleHandler::HandleStartSyncingWithEmail,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncStartKeyRetrieval",
      base::BindRepeating(&PeopleHandler::HandleStartKeyRetrieval,
                          base::Unretained(this)));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  web_ui()->RegisterMessageCallback(
      "GetChromeSigninUserChoiceInfo",
      base::BindRepeating(&PeopleHandler::HandleGetChromeSigninUserChoiceInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SetChromeSigninUserChoice",
      base::BindRepeating(&PeopleHandler::HandleSetChromeSigninUserChoice,
                          base::Unretained(this)));
#endif
}

void PeopleHandler::OnJavascriptAllowed() {
  PrefService* prefs = profile_->GetPrefs();
  profile_pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_pref_registrar_->Init(prefs);
  profile_pref_registrar_->Add(
      prefs::kSigninAllowed,
      base::BindRepeating(&PeopleHandler::UpdateSyncStatus,
                          base::Unretained(this)));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  SigninPrefs::ObserveSigninPrefsChanges(
      *profile_pref_registrar_,
      base::BindRepeating(&PeopleHandler::UpdateChromeSigninUserChoiceInfo,
                          base::Unretained(this)));
#endif

  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }

  // This is intentionally not using GetSyncService(), to go around the
  // Profile::IsSyncAllowed() check.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }
}

void PeopleHandler::OnJavascriptDisallowed() {
  profile_pref_registrar_.reset();
  identity_manager_observation_.Reset();
  sync_service_observation_.Reset();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PeopleHandler::DisplayGaiaLogin(signin_metrics::AccessPoint access_point) {
  // Advanced options are no longer being configured if the login screen is
  // visible. If the user exits the signin wizard after this without
  // configuring sync, CloseSyncSetup() will ensure they are logged out.
  configuring_sync_ = false;
  DisplayGaiaLoginInNewTabOrWindow(access_point);
}

void PeopleHandler::DisplayGaiaLoginInNewTabOrWindow(
    signin_metrics::AccessPoint access_point) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);

  syncer::SyncService* service = GetSyncService();
  if (service && service->HasUnrecoverableError() &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // When the user has an unrecoverable error, they first have to sign out and
    // then sign in again.
    identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
        signin_metrics::ProfileSignout::kRevokeSyncFromSettings);
  }

  // If the identity manager already has a primary account, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) ||
      signin_util::IsSigninPending(identity_manager)) {
    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(profile_);
    DCHECK(error_controller->HasError());
    signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(profile_,
                                                             access_point);
  } else {
    signin_ui_util::EnableSyncFromSingleAccountPromo(
        profile_, CoreAccountInfo(), access_point);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void PeopleHandler::OnDidClosePage(const base::Value::List& args) {
  // Don't mark setup as complete if "didAbort" is true, or if authentication
  // is still needed.
  if (!args[0].GetBool() && !IsProfileAuthNeededOrHasErrors()) {
    MarkFirstSetupComplete();
  }

  CloseSyncSetup();
}

syncer::SyncService* PeopleHandler::GetSyncService() const {
  return SyncServiceFactory::IsSyncAllowed(profile_)
             ? SyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void PeopleHandler::HandleSetDatatypes(const base::Value::List& args) {
  SyncConfigInfo configuration;
  const base::Value* callback_id = nullptr;
  ParseConfigurationArguments(args, &configuration, &callback_id);

  // Start configuring the SyncService using the configuration passed to us from
  // the JS layer.
  syncer::SyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just close the sync
  // dialog.
  if (!service || !service->IsEngineInitialized()) {
    CloseSyncSetup();
    ResolveJavascriptCallback(*callback_id, base::Value(kDonePageStatus));
    return;
  }

  // Don't enable non-registered types (for example, kApps may not be registered
  // on Chrome OS).
  configuration.selected_types.RetainAll(
      service->GetUserSettings()->GetRegisteredSelectableTypes());

  service->GetUserSettings()->SetSelectedTypes(configuration.sync_everything,
                                               configuration.selected_types);

  // Choosing data types to sync never fails.
  ResolveJavascriptCallback(*callback_id, base::Value(kConfigurePageStatus));
}

void PeopleHandler::HandleGetStoredAccounts(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetStoredAccountsList());
}

void PeopleHandler::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  UpdateStoredAccounts();
}

void PeopleHandler::OnExtendedAccountInfoRemoved(const AccountInfo& info) {
  UpdateStoredAccounts();
}

void PeopleHandler::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  UpdateChromeSigninUserChoiceInfo();
#endif
}

void PeopleHandler::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  UpdateChromeSigninUserChoiceInfo();
  UpdateSyncStatus();
#endif
}

void PeopleHandler::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin) ==
      account_info) {
    UpdateSyncStatus();
  }
}

base::Value::List PeopleHandler::GetStoredAccountsList() {
  base::Value::List accounts;
  bool populate_accounts_list = false;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  populate_accounts_list =
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  populate_accounts_list = !profile_->IsMainProfile();
#endif

  if (populate_accounts_list) {
    // If dice is enabled, show all the accounts.
    for (const auto& account : signin_ui_util::GetOrderedAccountsForDisplay(
             identity_manager,
             /*restrict_to_accounts_eligible_for_sync=*/true)) {
      accounts.Append(GetAccountValue(identity_manager, account));
    }
    return accounts;
  }

  // Guest mode does not have a primary account (or an IdentityManager).
  if (profile_->IsGuestSession()) {
    return base::Value::List();
  }
  // If DICE is disabled for this profile or unsupported on this platform (e.g.
  // Chrome OS) or Lacros main profile (sync with a different account than the
  // device account is not allowed), then show only the primary account,
  // whether or not that account has consented to sync.
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  if (!primary_account_info.IsEmpty()) {
    accounts.Append(GetAccountValue(identity_manager, primary_account_info));
  }
  return accounts;
}

void PeopleHandler::HandleStartSyncingWithEmail(const base::Value::List& args) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) ||
         AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile_));
  const base::Value& email = args[0];
  const base::Value& is_default_promo_account = args[1];

  DCHECK(IsChangePrimaryAccountAllowed(profile_, email.GetString()))
      << "Changing the primary account is not allowed!";

  AccountInfo maybe_account =
      IdentityManagerFactory::GetForProfile(profile_)
          ->FindExtendedAccountInfoByEmailAddress(email.GetString());
  signin_ui_util::EnableSyncFromMultiAccountPromo(
      profile_, maybe_account,
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
      is_default_promo_account.GetBool());
#else
  NOTIMPLEMENTED();
#endif
}

void PeopleHandler::HandleSetEncryptionPassphrase(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];

  // Check the SyncService is up and running before retrieving SyncUserSettings,
  // which contains the encryption-related APIs.
  if (!GetSyncService() || !GetSyncService()->IsEngineInitialized()) {
    // TODO(crbug.com/40725814): HandleSetDatatypes() also returns a success
    // status in this case. Consider returning a failure in both methods. Maybe
    // the CloseSyncSetup() call can also be removed.
    CloseSyncSetup();
    ResolveJavascriptCallback(callback_id, base::Value(true));
    return;
  }
  syncer::SyncUserSettings* sync_user_settings =
      GetSyncService()->GetUserSettings();

  const std::string& passphrase = args[1].GetString();
  bool successfully_set = false;
  if (passphrase.empty()) {
    successfully_set = false;
  } else if (!sync_user_settings->IsCustomPassphraseAllowed()) {
    successfully_set = false;
  } else if (sync_user_settings->IsUsingExplicitPassphrase()) {
    // In case a passphrase is already being used, changing to a new one isn't
    // currently supported (one must reset all the Sync data).
    successfully_set = false;
  } else if (sync_user_settings->IsPassphraseRequired() ||
             sync_user_settings->IsTrustedVaultKeyRequired()) {
    // Can't re-encrypt the data with |passphrase| if some of it hasn't even
    // been decrypted yet due to a pending passphrase / trusted vault key.
    successfully_set = false;
  } else {
    sync_user_settings->SetEncryptionPassphrase(passphrase);
    successfully_set = true;
  }
  ResolveJavascriptCallback(callback_id, base::Value(successfully_set));
}

void PeopleHandler::HandleSetDecryptionPassphrase(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];

  // Check the SyncService is up and running before retrieving SyncUserSettings,
  // which contains the encryption-related APIs.
  if (!GetSyncService() || !GetSyncService()->IsEngineInitialized()) {
    // TODO(crbug.com/40725814): HandleSetDatatypes() also returns a success
    // status in this case. Consider returning a failure in both methods. Maybe
    // the CloseSyncSetup() call can also be removed.
    CloseSyncSetup();
    ResolveJavascriptCallback(callback_id, base::Value(true));
    return;
  }
  syncer::SyncUserSettings* sync_user_settings =
      GetSyncService()->GetUserSettings();

  const std::string& passphrase = args[1].GetString();
  bool successfully_set = false;
  if (!passphrase.empty() && sync_user_settings->IsPassphraseRequired()) {
    successfully_set = sync_user_settings->SetDecryptionPassphrase(passphrase);
  }
  ResolveJavascriptCallback(callback_id, base::Value(successfully_set));
}

void PeopleHandler::HandleShowSyncSetupUI(const base::Value::List& args) {
  AllowJavascript();

  syncer::SyncService* service = GetSyncService();

  if (service && !sync_blocker_) {
    sync_blocker_ = service->GetSetupInProgressHandle();
  }

  // Mark Sync as requested by the user. It might already be requested, but
  // it's not if this is either the first time the user is setting up Sync, or
  // Sync was set up but then was reset via the dashboard. This also pokes the
  // SyncService to start up immediately, i.e. bypass deferred startup.
  if (service) {
    service->SetSyncFeatureRequested();
  }

  GetLoginUIService()->SetLoginUI(this);

  // Observe the web contents for a before unload event.
  Observe(web_ui()->GetWebContents());

  MaybeMarkSyncConfiguring();

  PushSyncPrefs();

  // Focus the web contents in case the location bar was focused before. This
  // makes sure that page elements for resolving sync errors can be focused.
  web_ui()->GetWebContents()->Focus();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// On ChromeOS, we need to sign out the user session to fix an auth error, so
// the user goes through the real signin flow to generate a new auth token.
void PeopleHandler::HandleAttemptUserExit(const base::Value::List& args) {
  DVLOG(1) << "Signing out the user to fix a sync error.";
  chrome::AttemptUserExit();
}

void PeopleHandler::HandleTurnOnSync(const base::Value::List& args) {
  NOTREACHED_IN_MIGRATION() << "It is not possible to toggle Sync on Ash";
}

void PeopleHandler::HandleTurnOffSync(const base::Value::List& args) {
  NOTREACHED_IN_MIGRATION() << "It is not possible to toggle Sync on Ash";
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PeopleHandler::HandleStartSignin(const base::Value::List& args) {
  AllowJavascript();

  // Should only be called if the user is not already signed in, has a auth
  // error, or a unrecoverable sync error requiring re-auth.
  syncer::SyncService* service = GetSyncService();
  DCHECK(IsProfileAuthNeededOrHasErrors() ||
         (service && service->HasUnrecoverableError()));
  DCHECK(IsChangePrimaryAccountAllowed(profile_, /*email=*/std::string()))
      << "Primary account already set and change is not allowed";

  DisplayGaiaLogin(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

void PeopleHandler::HandleSignout(const base::Value::List& args) {
  bool delete_profile = false;
  if (args[0].is_bool()) {
    delete_profile = args[0].GetBool();
  }
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  bool is_syncing =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  DCHECK(is_syncing || !delete_profile)
      << "Deleting the profile should only be offered if the user is "
         "syncing.";

  bool is_clear_primary_account_allowed =
      ChromeSigninClientFactory::GetForProfile(profile_)
          ->IsClearPrimaryAccountAllowed(is_syncing);

  if (is_syncing) {
    HandleTurnOffSync(delete_profile, is_clear_primary_account_allowed);
    return;
  }

  if (!is_clear_primary_account_allowed) {
    // 'Signout' should not be offered in the UI if clear primary account is
    // not allowed.
    NOTREACHED_IN_MIGRATION()
        << "Signout should not be offered if clear primary account is not "
           "allowed.";
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings);
#else
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  if (!browser) {
    return;
  }
  browser->signin_view_controller()->SignoutOrReauthWithPrompt(
      signin_metrics::AccessPoint::
          ACCESS_POINT_SETTINGS_SIGNOUT_CONFIRMATION_PROMPT,
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
      signin_metrics::SourceForRefreshTokenOperation::kSettings_Signout);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void PeopleHandler::HandleTurnOffSync(bool delete_profile,
                                      bool is_clear_primary_account_allowed) {
  base::FilePath profile_path = profile_->GetPath();
  bool delete_profile_allowed = signin_util::IsProfileDeletionAllowed(profile_);
  DCHECK(!delete_profile || delete_profile_allowed)
      << "Profile deletion is not allowed!";

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  auto* signin_client = ChromeSigninClientFactory::GetForProfile(profile_);

  if (!signin_client->IsRevokeSyncConsentAllowed()) {
    // If the user can't revoke sync the profile must be destroyed.
    if (delete_profile && delete_profile_allowed) {
      webui::DeleteProfileAtPath(profile_path,
                                 ProfileMetrics::DELETE_PROFILE_SETTINGS);
    } else {
      DCHECK(delete_profile) << "User signout requires profile destruction.";
    }
    return;
  }

  if (!is_clear_primary_account_allowed) {
    DCHECK(signin_client->IsRevokeSyncConsentAllowed());
    identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
        signin_metrics::ProfileSignout::kRevokeSyncFromSettings);
  } else {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::ProfileSignout::kUserClickedSignoutSettings);
#else
    Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
    if (browser) {
      // Clearing the primary account isn't sufficient to signout SAML accounts,
      // see http://crbug.com/1114646.
      browser->signin_view_controller()->ShowGaiaLogoutTab(
          signin_metrics::SourceForRefreshTokenOperation::kSettings_Signout);
    }

    if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
      // In Uno, Gaia logout tab invalidating the account will lead to a sign in
      // paused state. Unset the primary account to ensure it is removed from
      // chrome. The `AccountReconcilor` will revoke refresh tokens for accounts
      // not in the Gaia cookie on next reconciliation.
      identity_manager->GetPrimaryAccountMutator()
          ->RemovePrimaryAccountButKeepTokens(
              signin_metrics::ProfileSignout::kUserClickedSignoutSettings);
    } else {
      // Only revoke the sync consent.
      // * If the primary account is still valid, then it will be removed by
      // the Gaia logout tab (see http://crbug.com/1068978).
      // * If the account is already invalid, drop the token now because it's
      // already invalid on the web, so the Gaia logout tab won't affect it
      // (see http://crbug.com/1114646).
      //
      // This operation may delete the current browser that owns |this| if force
      // signin is enabled (see https://crbug.com/1153120).
      identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
          signin_metrics::ProfileSignout::kRevokeSyncFromSettings);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  // CAUTION: |this| may be deleted at this point.
  if (delete_profile && delete_profile_allowed) {
    webui::DeleteProfileAtPath(profile_path,
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void PeopleHandler::HandlePauseSync(const base::Value::List& args) {
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_));
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  identity_manager->GetAccountsMutator()
      ->InvalidateRefreshTokenForPrimaryAccount(
          signin_metrics::SourceForRefreshTokenOperation::kSettings_PauseSync);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

void PeopleHandler::HandleStartKeyRetrieval(const base::Value::List& args) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::FeatureList::IsEnabled(
          trusted_vault::kChromeOSTrustedVaultUseWebUIDialog)) {
    OpenDialogForSyncKeyRetrieval(
        profile_, syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
    return;
  }
#endif

  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  if (!browser) {
    return;
  }

  OpenTabForSyncKeyRetrieval(
      browser, syncer::TrustedVaultUserActionTriggerForUMA::kSettings);
}

void PeopleHandler::HandleGetSyncStatus(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetSyncStatusDictionary());
}

void PeopleHandler::HandleSyncPrefsDispatch(const base::Value::List& args) {
  AllowJavascript();
  PushSyncPrefs();
}

void PeopleHandler::HandleTrustedVaultBannerStateDispatch(
    const base::Value::List& args) {
  AllowJavascript();
  PushTrustedVaultBannerState();
}

void PeopleHandler::CloseSyncSetup() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  // LoginUIService can be nullptr if page is brought up in incognito mode
  // (i.e. if the user is running in guest mode in cros and brings up settings).
  LoginUIService* service = GetLoginUIService();
  if (service) {
    auto self_weak_ptr = weak_factory_.GetWeakPtr();

    // ChromeOS Ash doesn't support signing out and hence the code below
    // cannot build (RevokeSyncConsent() doesn't exist). However, the code is
    // unreachable on Ash because IsInitialSyncFeatureSetupComplete() in the
    // condition below always returns true.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    syncer::SyncService* sync_service = GetSyncService();

    // Don't log a cancel event if the sync setup dialog is being
    // automatically closed due to an auth error.
    if (service->current_login_ui() == this && sync_service &&
        configuring_sync_ &&
        !sync_service->GetUserSettings()->IsInitialSyncFeatureSetupComplete() &&
        sync_service->GetAuthError().state() == GoogleServiceAuthError::NONE) {
      DVLOG(1) << "Sync setup aborted by user action";

      // Revoke sync consent on desktop Chrome if they click cancel during
      // initial setup or close sync setup without confirming sync.
      IdentityManagerFactory::GetForProfile(profile_)
          ->GetPrimaryAccountMutator()
          ->RevokeSyncConsent(signin_metrics::ProfileSignout::kAbortSignin);
    }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

    service->LoginUIClosed(this);

    // The call to RevokeSyncConsent() above may delete the current browser that
    // owns `this` if force signin is enabled. Accessing instance members caused
    // crashes (see https://crbug.com/1441820) which we guard against by
    // checking a weak pointer to the current instance.
    if (!self_weak_ptr) {
      return;
    }
  }

  // Alert the sync service anytime the sync setup dialog is closed. This can
  // happen due to the user clicking the OK or Cancel button, or due to the
  // dialog being closed by virtue of sync being disabled in the background.
  sync_blocker_.reset();

  configuring_sync_ = false;

  // Stop observing the web contents.
  Observe(nullptr);
}

void PeopleHandler::InitializeSyncBlocker() {
  DCHECK(web_ui());
  WebContents* web_contents = web_ui()->GetWebContents();
  if (!web_contents) {
    return;
  }

  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  // The user opened settings directly to the syncSetup sub-page, because they
  // clicked "Settings" in the browser sync consent dialog or because they
  // clicked "Review sync options" in the Chrome OS out-of-box experience.
  // Don't start syncing until they finish setup.
  if (IsSyncSubpage(web_contents->GetVisibleURL())) {
    sync_blocker_ = service->GetSetupInProgressHandle();
  }
}

void PeopleHandler::FocusUI() {
  WebContents* web_contents = web_ui()->GetWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void PeopleHandler::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // After a primary account was set, the Sync setup will start soon. Grab a
      // SetupInProgressHandle right now to avoid a temporary "missing Sync
      // confirmation" error in the avatar menu. See crbug.com/928696.
      syncer::SyncService* service = GetSyncService();
      if (service && !sync_blocker_) {
        sync_blocker_ = service->GetSetupInProgressHandle();
      }
      UpdateSyncStatus();
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      sync_blocker_.reset();
      configuring_sync_ = false;
      UpdateSyncStatus();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      UpdateChromeSigninUserChoiceInfo();
      UpdateStoredAccounts();
      UpdateSyncStatus();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
#endif
}

void PeopleHandler::OnStateChanged(syncer::SyncService* sync_service) {
  UpdateSyncStatus();
  // TODO(crbug.com/40140566): Re-evaluate marking sync as configuring here,
  // since this gets called whenever SyncService changes state. Inline
  // MaybeMarkSyncConfiguring() then.
  MaybeMarkSyncConfiguring();
  PushSyncPrefs();
  PushTrustedVaultBannerState();
}

void PeopleHandler::BeforeUnloadDialogCancelled() {
  // The before unload dialog is only shown during the first sync setup.
  DCHECK(IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  syncer::SyncService* service = GetSyncService();
  DCHECK(service && service->IsSetupInProgress() &&
         !service->GetUserSettings()->IsInitialSyncFeatureSetupComplete());

  base::RecordAction(
      base::UserMetricsAction("Signin_Signin_CancelAbortAdvancedSyncSettings"));
}

base::Value::Dict PeopleHandler::GetSyncStatusDictionary() const {
  base::Value::Dict sync_status;
  if (profile_->IsGuestSession()) {
    // Cannot display signin status when running in guest mode on chromeos
    // because there is no IdentityManager.
    return sync_status;
  }

  sync_status.Set("supervisedUser", profile_->IsChild());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(identity_manager);

  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    CoreAccountInfo primary_account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

    // If there is no one logged in or if the profile name is empty then the
    // domain name is empty. This happens in browser tests.
    if (enterprise_util::UserAcceptedAccountManagement(profile_) &&
        !primary_account_info.email.empty()) {
      sync_status.Set("domain",
                      gaia::ExtractDomainName(primary_account_info.email));
    }
  }

  // This is intentionally not using GetSyncService(), in order to access more
  // nuanced information, since GetSyncService() returns nullptr if anything
  // makes Profile::IsSyncAllowed() false.
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  bool disallowed_by_policy =
      service && service->HasDisableReason(
                     syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  sync_status.Set("syncSystemEnabled", (service != nullptr));
  sync_status.Set(
      "firstSetupInProgress",
      service && !disallowed_by_policy && service->IsSetupInProgress() &&
          !service->GetUserSettings()->IsInitialSyncFeatureSetupComplete() &&
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  const SyncStatusLabels status_labels = GetSyncStatusLabels(profile_);
  // TODO(crbug.com/40660240): Consider unifying some of the fields below to
  // avoid redundancy.
  sync_status.Set("statusText",
                  GetStringUTF16(status_labels.status_label_string_id));
  sync_status.Set("statusActionText",
                  GetStringUTF16(status_labels.button_string_id));
  sync_status.Set(
      "hasError",
      status_labels.message_type == SyncStatusMessageType::kSyncError ||
          status_labels.message_type ==
              SyncStatusMessageType::kPasswordsOnlySyncError);
  sync_status.Set("hasPasswordsOnlyError",
                  status_labels.message_type ==
                      SyncStatusMessageType::kPasswordsOnlySyncError);
  sync_status.Set("statusAction",
                  GetSyncErrorAction(status_labels.action_type));

  sync_status.Set("managed", disallowed_by_policy);
  // TODO(crbug.com/40745012): audit js usages of |disabled| and |signedInState|
  // (sync part) fields, update it to use the right field, comments around and
  // conditions here. Perhaps removal of one of these to fields is possible.
  sync_status.Set("disabled", !service || disallowed_by_policy);
  // `kSyncPaused` and `kSyncing` are currently equivalent to only `kSyncing` in
  // settings. `kSyncPaused` state is identified with having
  // `syncStatus.hasError: true` as well.
  // TODO(b/336510160): Look into integrating kSyncPaused value, potentially by
  // merging it with the `hasError` message.
  signin_util::SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager);
  sync_status.Set("signedInState",
                  static_cast<int>(
                      signed_in_state == signin_util::SignedInState::kSyncPaused
                          ? signin_util::SignedInState::kSyncing
                          : signed_in_state));
  sync_status.Set("signedInUsername",
                  signin_ui_util::GetAuthenticatedUsername(profile_));
  sync_status.Set("hasUnrecoverableError",
                  service && service->HasUnrecoverableError());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsFloatingSsoAllowed()) {
    sync_status.Set("syncCookiesSupported", profile_->GetPrefs()->GetBoolean(
                                                prefs::kFloatingSsoEnabled));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return sync_status;
}

void PeopleHandler::PushSyncPrefs() {
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service || !service->IsEngineInitialized()) {
    return;
  }

  // Setup values for the JSON response:
  //   syncAllDataTypes: true if the user wants to sync everything
  //   <data_type>Registered: true if the associated data type is supported
  //   <data_type>Synced: true if the user wants to sync that specific data type
  //   customPassphraseAllowed: true if sync allows setting a custom passphrase
  //                            to encrypt data.
  //   encryptAllData: true if user wants to encrypt all data (not just
  //       passwords)
  //   passphraseRequired: true if a passphrase is needed to start sync
  //   trustedVaultKeysRequired: true if trusted vault keys are needed to start
  //                             sync.
  //   explicitPassphraseTime: the stringified time when the current explicit
  //                   passphrase was set (in milliseconds since the Unix
  //                   epoch); undefined if the time is unknown or no explicit
  //                   passphrase is set.
  //
  base::Value::Dict args;

  syncer::SyncUserSettings* sync_user_settings = service->GetUserSettings();
  // Tell the UI layer which data types are registered/enabled by the user.
  const syncer::UserSelectableTypeSet registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  const syncer::UserSelectableTypeSet selected_types =
      sync_user_settings->GetSelectedTypes();
  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    const std::string type_name = syncer::GetUserSelectableTypeName(type);
    args.Set(type_name + "Registered", registered_types.Has(type));
    args.Set(type_name + "Synced", selected_types.Has(type));
    args.Set(type_name + "Managed",
             sync_user_settings->IsTypeManagedByPolicy(type));
  }
  args.Set("syncAllDataTypes", sync_user_settings->IsSyncEverythingEnabled());
  args.Set("encryptAllData", sync_user_settings->IsEncryptEverythingEnabled());
  args.Set("customPassphraseAllowed",
           sync_user_settings->IsCustomPassphraseAllowed());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForPreferredDataTypes(), because we want to show the
  // passphrase UI even if no encrypted data types are enabled.
  // IsInitialSyncFeatureSetupComplete()==false is special-cased to avoid that
  // the user enters the custom passphrase before confirming they want to
  // complete the sync setup flow.
  args.Set("passphraseRequired",
           sync_user_settings->IsPassphraseRequired() &&
               sync_user_settings->IsInitialSyncFeatureSetupComplete());

  // Same as above, we call IsTrustedVaultKeyRequired() here instead of.
  // IsTrustedVaultKeyRequiredForPreferredDataTypes().
  args.Set("trustedVaultKeysRequired",
           sync_user_settings->IsTrustedVaultKeyRequired());

  base::Time passphrase_time = sync_user_settings->GetExplicitPassphraseTime();
  if (!passphrase_time.is_null()) {
    args.Set("explicitPassphraseTime",
             base::TimeFormatShortDate(passphrase_time));
  }

  FireWebUIListener("sync-prefs-changed", args);
}

void PeopleHandler::PushTrustedVaultBannerState() {
  syncer::SyncService* sync_service = GetSyncService();
  auto state = TrustedVaultBannerState::kNotShown;
  if (sync_service && sync_service->GetUserSettings()->GetPassphraseType() ==
                          syncer::PassphraseType::kTrustedVaultPassphrase) {
    state = TrustedVaultBannerState::kOptedIn;
  } else if (syncer::ShouldOfferTrustedVaultOptIn(sync_service)) {
    state = TrustedVaultBannerState::kOfferOptIn;
  }

  FireWebUIListener(kTrustedVaultBannerStateChangedEvent,
                    base::Value(static_cast<int>(state)));
}

LoginUIService* PeopleHandler::GetLoginUIService() const {
  return LoginUIServiceFactory::GetForProfile(profile_);
}

void PeopleHandler::UpdateSyncStatus() {
  FireWebUIListener("sync-status-changed", GetSyncStatusDictionary());
}

void PeopleHandler::UpdateStoredAccounts() {
  FireWebUIListener("stored-accounts-updated", GetStoredAccountsList());
}

void PeopleHandler::MarkFirstSetupComplete() {
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service) {
    return;
  }

  // Sync is usually already requested at this point, but it might not be if
  // Sync was reset from the dashboard while this page was open. (In most
  // situations, resetting Sync also signs the user out of Chrome so this
  // doesn't come up, but on ChromeOS or for managed (enterprise) accounts
  // signout isn't possible.)
  // Note that this has to happen *before* checking if first-time setup is
  // already marked complete, because on some platforms (e.g. ChromeOS) that
  // gets set automatically.
  service->SetSyncFeatureRequested();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // If the first-time setup is already complete, there's nothing else to do.
  if (service->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
    return;
  }

  unified_consent::metrics::RecordSyncSetupDataTypesHistrogam(
      service->GetUserSettings());

  // We're done configuring, so notify SyncService that it is OK to start
  // syncing.
  service->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM);
  FireWebUIListener("sync-settings-saved");
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void PeopleHandler::MaybeMarkSyncConfiguring() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsProfileAuthNeededOrHasErrors()) {
    return;
  }
#endif
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (service && service->IsEngineInitialized()) {
    configuring_sync_ = true;
  }
}

bool PeopleHandler::IsProfileAuthNeededOrHasErrors() {
  return !IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
             signin::ConsentLevel::kSync) ||
         SigninErrorControllerFactory::GetForProfile(profile_)->HasError();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
base::Value::Dict PeopleHandler::GetChromeSigninUserChoiceInfo() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  // Gets the Chrome signed in account or the first signed in account in the
  // cooke jar, refresh token should be available too.
  AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);

  bool should_show_settings =
      !signin::IsImplicitBrowserSigninOrExplicitDisabled(
          identity_manager, profile_->GetPrefs()) &&
      !account.IsEmpty();

  ChromeSigninUserChoice choice =
      should_show_settings
          ? SigninPrefs(*profile_->GetPrefs())
                .GetChromeSigninInterceptionUserChoice(account.gaia)
          : ChromeSigninUserChoice::kNoChoice;

  // Set for metrics purposes.
  chrome_signin_user_choice_shown_ |= should_show_settings;

  base::Value::Dict chrome_signin_user_choice_info;
  chrome_signin_user_choice_info.Set("shouldShowSettings",
                                     should_show_settings);
  chrome_signin_user_choice_info.Set("choice", static_cast<int>(choice));
  chrome_signin_user_choice_info.Set("signedInEmail", account.email);

  return chrome_signin_user_choice_info;
}

void PeopleHandler::HandleGetChromeSigninUserChoiceInfo(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  ResolveJavascriptCallback(args[0], GetChromeSigninUserChoiceInfo());
}

void PeopleHandler::HandleSetChromeSigninUserChoice(
    const base::Value::List& args) {
  CHECK(!signin::IsImplicitBrowserSigninOrExplicitDisabled(
      IdentityManagerFactory::GetForProfile(profile_), profile_->GetPrefs()));
  CHECK_EQ(2U, args.size());

  CHECK(args[0].is_int());
  ChromeSigninUserChoice user_choice =
      static_cast<ChromeSigninUserChoice>(args[0].GetInt());
  CHECK_NE(user_choice, ChromeSigninUserChoice::kNoChoice);

  CHECK(args[1].is_string());
  std::string signed_in_email = args[1].GetString();
  CHECK(!signed_in_email.empty());

  AccountInfo account =
      IdentityManagerFactory::GetForProfile(profile_)
          ->FindExtendedAccountInfoByEmailAddress(signed_in_email);
  SigninPrefs signin_prefs(*profile_->GetPrefs());
  // Early return to avoid recording histogram settings modifications. Also
  // guarantees that the `user_choice` is from a user modification through the
  // UI since the `SigninPrefs` is not aware of it yet.
  if (user_choice ==
      signin_prefs.GetChromeSigninInterceptionUserChoice(account.gaia)) {
    return;
  }

  signin_prefs.SetChromeSigninInterceptionUserChoice(account.gaia, user_choice);
  // If the user explicitly set the `kDoNotSignin` choice from the settings,
  // suppress any bubble interaction time that could lead to re-prompts.
  if (user_choice == ChromeSigninUserChoice::kDoNotSignin) {
    signin_prefs.ClearChromeSigninInterceptionLastBubbleDeclineTime(
        account.gaia);
    signin_prefs.ClearChromeSigninBubbleRepromptCount(account.gaia);
  }

  // Set for metrics purposes.
  chrome_signin_user_choice_modified_ = true;
  base::UmaHistogramEnumeration(
      "Signin.Settings.ChromeSigninSettingModification",
      ChromeSigninUserChoiceToModification(user_choice));
}

void PeopleHandler::UpdateChromeSigninUserChoiceInfo() {
  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    FireWebUIListener("chrome-signin-user-choice-info-change",
                      GetChromeSigninUserChoiceInfo());
  }
}

void PeopleHandler::HandleSetChromeSigninUserChoiceForTesting(
    const std::string& email,
    ChromeSigninUserChoice choice) {
  base::Value::List args;
  args.Append(static_cast<int>(choice));
  args.Append(email);
  HandleSetChromeSigninUserChoice(args);
}
#endif

}  // namespace settings
