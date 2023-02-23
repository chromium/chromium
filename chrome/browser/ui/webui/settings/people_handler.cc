// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <string>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
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
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
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

// A structure which contains all the configuration information for sync.
struct SyncConfigInfo {
  SyncConfigInfo();
  ~SyncConfigInfo();

  bool sync_everything;
  syncer::UserSelectableTypeSet selected_types;
  bool payments_integration_enabled;
};

bool IsSyncSubpage(const GURL& current_url) {
  return current_url == chrome::GetSettingsUrl(chrome::kSyncSetupSubPage);
}

SyncConfigInfo::SyncConfigInfo()
    : sync_everything(false), payments_integration_enabled(false) {}

SyncConfigInfo::~SyncConfigInfo() {}

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  absl::optional<base::Value> parsed_value = base::JSONReader::Read(json);
  if (!parsed_value.has_value() || !parsed_value->is_dict()) {
    DLOG(ERROR) << "GetConfiguration() not passed a Dictionary";
    return false;
  }

  const base::Value::Dict& root = parsed_value->GetDict();
  absl::optional<bool> sync_everything = root.FindBool("syncAllDataTypes");
  if (!sync_everything.has_value()) {
    DLOG(ERROR) << "GetConfiguration() not passed a syncAllDataTypes value";
    return false;
  }
  config->sync_everything = *sync_everything;

  absl::optional<bool> payments_integration_enabled =
      root.FindBool("paymentsIntegrationEnabled");
  if (!payments_integration_enabled.has_value()) {
    DLOG(ERROR) << "GetConfiguration() not passed a paymentsIntegrationEnabled "
                << "value";
    return false;
  }
  config->payments_integration_enabled = *payments_integration_enabled;

  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    std::string key_name =
        syncer::GetUserSelectableTypeName(type) + std::string("Synced");
    absl::optional<bool> type_synced = root.FindBool(key_name);
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
  if ((*callback_id = &args[0]) && !json.empty())
    CHECK(GetConfiguration(json, config));
  else
    NOTREACHED();
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

  NOTREACHED();
  return std::string();
}

// Returns the base::Value associated with the account, to use in the stored
// accounts list.
base::Value::Dict GetAccountValue(const AccountInfo& account) {
  DCHECK(!account.IsEmpty());
  base::Value::Dict dict;
  dict.Set("email", account.email);
  dict.Set("fullName", account.full_name);
  dict.Set("givenName", account.given_name);
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
}  // namespace

namespace settings {

// static
const char PeopleHandler::kConfigurePageStatus[] = "configure";
const char PeopleHandler::kDonePageStatus[] = "done";
const char PeopleHandler::kPassphraseFailedPageStatus[] = "passphraseFailed";

PeopleHandler::PeopleHandler(Profile* profile)
    : profile_(profile), configuring_sync_(false) {}

PeopleHandler::~PeopleHandler() {
  // Early exit if running unit tests (no actual WebUI is attached).
  if (!web_ui())
    return;

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
}

void PeopleHandler::OnJavascriptAllowed() {
  PrefService* prefs = profile_->GetPrefs();
  profile_pref_registrar_.Init(prefs);
  profile_pref_registrar_.Add(
      prefs::kSigninAllowed,
      base::BindRepeating(&PeopleHandler::UpdateSyncStatus,
                          base::Unretained(this)));

  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager)
    identity_manager_observation_.Observe(identity_manager);

  // This is intentionally not using GetSyncService(), to go around the
  // Profile::IsSyncAllowed() check.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service_observation_.Observe(sync_service);
}

void PeopleHandler::OnJavascriptDisallowed() {
  profile_pref_registrar_.RemoveAll();
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
        signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
        signin_metrics::SignoutDelete::kIgnoreMetric);
  }

  // If the identity manager already has a primary account, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
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

  autofill::prefs::SetPaymentsIntegrationEnabled(
      profile_->GetPrefs(), configuration.payments_integration_enabled);

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

  ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CUSTOMIZE);
  if (!configuration.sync_everything)
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CHOOSE);
}

void PeopleHandler::HandleGetStoredAccounts(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetStoredAccountsList());
}

void PeopleHandler::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-updated", GetStoredAccountsList());
}

void PeopleHandler::OnExtendedAccountInfoRemoved(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-updated", GetStoredAccountsList());
}

base::Value::List PeopleHandler::GetStoredAccountsList() {
  base::Value::List accounts;
  bool populate_accounts_list = false;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  populate_accounts_list =
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  populate_accounts_list = !profile_->IsMainProfile();
#endif

  if (populate_accounts_list) {
    // If dice is enabled, show all the accounts.
    for (const auto& account : signin_ui_util::GetOrderedAccountsForDisplay(
             profile_, /*restrict_to_accounts_eligible_for_sync=*/true)) {
      accounts.Append(GetAccountValue(account));
    }
    return accounts;
  }

  // Guest mode does not have a primary account (or an IdentityManager).
  if (profile_->IsGuestSession())
    return base::Value::List();
  // If DICE is disabled for this profile or unsupported on this platform (e.g.
  // Chrome OS) or Lacros main profile (sync with a different account than the
  // device account is not allowed), then show only the primary account,
  // whether or not that account has consented to sync.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  if (!primary_account_info.IsEmpty())
    accounts.Append(GetAccountValue(primary_account_info));
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
    // TODO(crbug.com/1139060): HandleSetDatatypes() also returns a success
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
    ProfileMetrics::LogProfileSyncInfo(
        ProfileMetrics::SYNC_CREATED_NEW_PASSPHRASE);
  }
  ResolveJavascriptCallback(callback_id, base::Value(successfully_set));
}

void PeopleHandler::HandleSetDecryptionPassphrase(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];

  // Check the SyncService is up and running before retrieving SyncUserSettings,
  // which contains the encryption-related APIs.
  if (!GetSyncService() || !GetSyncService()->IsEngineInitialized()) {
    // TODO(crbug.com/1139060): HandleSetDatatypes() also returns a success
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
    if (successfully_set) {
      ProfileMetrics::LogProfileSyncInfo(
          ProfileMetrics::SYNC_ENTERED_EXISTING_PASSPHRASE);
    }
  }
  ResolveJavascriptCallback(callback_id, base::Value(successfully_set));
}

void PeopleHandler::HandleShowSyncSetupUI(const base::Value::List& args) {
  AllowJavascript();

  syncer::SyncService* service = GetSyncService();

  if (service && !sync_blocker_)
    sync_blocker_ = service->GetSetupInProgressHandle();

  // Mark Sync as requested by the user. It might already be requested, but
  // it's not if this is either the first time the user is setting up Sync, or
  // Sync was set up but then was reset via the dashboard. This also pokes the
  // SyncService to start up immediately, i.e. bypass deferred startup.
  if (service)
    service->GetUserSettings()->SetSyncRequested(true);

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
  NOTREACHED() << "It is not possible to toggle Sync on Ash";
}

void PeopleHandler::HandleTurnOffSync(const base::Value::List& args) {
  NOTREACHED() << "It is not possible to toggle Sync on Ash";
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
  if (args[0].is_bool())
    delete_profile = args[0].GetBool();
  base::FilePath profile_path = profile_->GetPath();

  // TODO(crbug.com/1315163): consider splitting `HandleSignout()` in two
  // different functions: one for "Signout" and one for "Turn off".
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  bool is_syncing =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  bool delete_profile_allowed = signin_util::IsProfileDeletionAllowed(profile_);

  DCHECK(!delete_profile || delete_profile_allowed)
      << "Profile deletion is not allowed!";
  DCHECK(is_syncing || !delete_profile)
      << "Deleting the profile should only be offered if the user is "
         "syncing.";
  auto* signin_client = ChromeSigninClientFactory::GetForProfile(profile_);
  if (is_syncing && !signin_client->IsRevokeSyncConsentAllowed()) {
    // If the user can't revoke sync the profile must be destroyed.
    if (delete_profile && delete_profile_allowed) {
      webui::DeleteProfileAtPath(profile_path,
                                 ProfileMetrics::DELETE_PROFILE_SETTINGS);
    } else {
      DCHECK(delete_profile) << "User signout requires profile destruction.";
    }
    return;
  }

  bool is_clear_primary_account_allowed =
      signin_client->IsClearPrimaryAccountAllowed(is_syncing);
  if (!is_syncing && !is_clear_primary_account_allowed) {
    // 'Signout' should not be offered in the UI if clear primary account is not
    // allowed.
    NOTREACHED()
        << "Signout should not be offered if clear primary account is not "
           "allowed.";
    return;
  }

  signin_metrics::SignoutDelete delete_metric =
      delete_profile ? signin_metrics::SignoutDelete::kDeleted
                     : signin_metrics::SignoutDelete::kKeeping;

  if (is_syncing && !is_clear_primary_account_allowed) {
    DCHECK(signin_client->IsRevokeSyncConsentAllowed());
    identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
        signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
        delete_metric);
  } else {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
        delete_metric);
#else
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  if (browser) {
    browser->signin_view_controller()->ShowGaiaLogoutTab(
        signin_metrics::SourceForRefreshTokenOperation::kSettings_Signout);
  }

  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
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
        signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
        delete_metric);
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
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  if (!browser)
    return;

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
    syncer::SyncService* sync_service = GetSyncService();

    // Don't log a cancel event if the sync setup dialog is being
    // automatically closed due to an auth error.
    if ((service->current_login_ui() == this) &&
        (!sync_service ||
         (!sync_service->GetUserSettings()->IsFirstSetupComplete() &&
          sync_service->GetAuthError().state() ==
              GoogleServiceAuthError::NONE))) {
      if (configuring_sync_) {
        // If the user clicked "Cancel" while setting up sync, disable sync
        // because we don't want the sync engine to remain in the
        // first-setup-incomplete state.
        // Note: In order to disable sync across restarts on Chrome OS,
        // we must call StopAndClear(), which suppresses sync startup in
        // addition to disabling it.
        if (sync_service) {
          DVLOG(1) << "Sync setup aborted by user action";
          sync_service->StopAndClear();
// ChromeOS ash doesn't support signing out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
          // Revoke sync consent on desktop Chrome if they click cancel during
          // initial setup or close sync setup without confirming sync.
          if (!sync_service->GetUserSettings()->IsFirstSetupComplete()) {
            IdentityManagerFactory::GetForProfile(profile_)
                ->GetPrimaryAccountMutator()
                ->RevokeSyncConsent(
                    signin_metrics::ProfileSignout::kAbortSignin,
                    signin_metrics::SignoutDelete::kIgnoreMetric);
          }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
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

  // Stop observing the web contents.
  Observe(nullptr);
}

void PeopleHandler::InitializeSyncBlocker() {
  DCHECK(web_ui());
  WebContents* web_contents = web_ui()->GetWebContents();
  if (!web_contents)
    return;

  syncer::SyncService* service = GetSyncService();
  if (!service)
    return;

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
      if (service && !sync_blocker_)
        sync_blocker_ = service->GetSetupInProgressHandle();
      UpdateSyncStatus();
      return;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      sync_blocker_.reset();
      configuring_sync_ = false;
      UpdateSyncStatus();
      return;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
  }
}

void PeopleHandler::OnStateChanged(syncer::SyncService* sync_service) {
  UpdateSyncStatus();
  // TODO(crbug.com/1106764): Re-evaluate marking sync as configuring here,
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
         !service->GetUserSettings()->IsFirstSetupComplete());

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
  sync_status.Set("childUser", profile_->IsChild());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(identity_manager);

  // TODO(crbug.com/1369982): |domain| is used to show the profile deletion
  // dialog on turn off sync. This is no longer needed since users are allowed
  // to turn off sync. Enterprise team to decide whether to show the delete
  // profile dialog on signout.
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    CoreAccountInfo primary_account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

    // If there is no one logged in or if the profile name is empty then the
    // domain name is empty. This happens in browser tests.
    if (chrome::enterprise_util::UserAcceptedAccountManagement(profile_) &&
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
          !service->GetUserSettings()->IsFirstSetupComplete() &&
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  const SyncStatusLabels status_labels = GetSyncStatusLabels(profile_);
  // TODO(crbug.com/1027467): Consider unifying some of the fields below to
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
  // TODO(crbug.com/1171279): audit js usages of |disabled| and |signedIn|
  // fields, update it to use the right field, comments around and conditions
  // here. Perhaps removal of one of these to fields is possible.
  sync_status.Set("disabled", !service || disallowed_by_policy);
  // NOTE: This means signed-in for *sync*. It can be false when the user is
  // signed-in to the content area or to the browser.
  sync_status.Set("signedIn", identity_manager->HasPrimaryAccount(
                                  signin::ConsentLevel::kSync));
  sync_status.Set("signedInUsername",
                  signin_ui_util::GetAuthenticatedUsername(profile_));
  sync_status.Set("hasUnrecoverableError",
                  service && service->HasUnrecoverableError());
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
  //   paymentsIntegrationEnabled: true if the user wants Payments integration
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
  }
  args.Set("syncAllDataTypes", sync_user_settings->IsSyncEverythingEnabled());
  args.Set("paymentsIntegrationEnabled",
           autofill::prefs::IsPaymentsIntegrationEnabled(profile_->GetPrefs()));
  args.Set("encryptAllData", sync_user_settings->IsEncryptEverythingEnabled());
  args.Set("customPassphraseAllowed",
           sync_user_settings->IsCustomPassphraseAllowed());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForPreferredDataTypes(), because we want to show the
  // passphrase UI even if no encrypted data types are enabled.
  args.Set("passphraseRequired", sync_user_settings->IsPassphraseRequired());

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

void PeopleHandler::MarkFirstSetupComplete() {
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service)
    return;

  // Sync is usually already requested at this point, but it might not be if
  // Sync was reset from the dashboard while this page was open. (In most
  // situations, resetting Sync also signs the user out of Chrome so this
  // doesn't come up, but on ChromeOS or for managed (enterprise) accounts
  // signout isn't possible.)
  // Note that this has to happen *before* checking if first-time setup is
  // already marked complete, because on some platforms (e.g. ChromeOS) that
  // gets set automatically.
  service->GetUserSettings()->SetSyncRequested(true);

  // If the first-time setup is already complete, there's nothing else to do.
  if (service->GetUserSettings()->IsFirstSetupComplete())
    return;

  unified_consent::metrics::RecordSyncSetupDataTypesHistrogam(
      service->GetUserSettings(), profile_->GetPrefs());

  // We're done configuring, so notify SyncService that it is OK to start
  // syncing.
  service->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM);
  FireWebUIListener("sync-settings-saved");
}

void PeopleHandler::MaybeMarkSyncConfiguring() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsProfileAuthNeededOrHasErrors())
    return;
#endif
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (service && service->IsEngineInitialized())
    configuring_sync_ = true;
}

bool PeopleHandler::IsProfileAuthNeededOrHasErrors() {
  return !IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
             signin::ConsentLevel::kSync) ||
         SigninErrorControllerFactory::GetForProfile(profile_)->HasError();
}

}  // namespace settings
