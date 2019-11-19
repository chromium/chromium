// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
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
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#else
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#endif

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
  syncer::UserSelectableTypeSet selected_types;
  bool payments_integration_enabled;
  std::string passphrase;
  bool set_new_passphrase;
};

bool IsSyncSubpage(const GURL& current_url) {
  return (current_url == chrome::GetSettingsUrl(chrome::kSyncSetupSubPage)
#if defined(OS_CHROMEOS)
          || current_url == chrome::GetOSSettingsUrl(chrome::kSyncSetupSubPage)
#endif  // defined(OS_CHROMEOS)
  );
}

SyncConfigInfo::SyncConfigInfo()
    : encrypt_all(false),
      sync_everything(false),
      payments_integration_enabled(false),
      set_new_passphrase(false) {}

SyncConfigInfo::~SyncConfigInfo() {}

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  std::unique_ptr<base::Value> parsed_value =
      base::JSONReader::ReadDeprecated(json);
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

  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    std::string key_name =
        syncer::GetUserSelectableTypeName(type) + std::string("Synced");
    bool sync_value;
    if (!result->GetBoolean(key_name, &sync_value)) {
      DLOG(ERROR) << "GetConfiguration() not passed a value for " << key_name;
      return false;
    }
    if (sync_value)
      config->selected_types.Put(type);
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
base::Value GetAccountValue(const AccountInfo& account) {
  DCHECK(!account.IsEmpty());
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetKey("email", base::Value(account.email));
  dictionary.SetKey("fullName", base::Value(account.full_name));
  dictionary.SetKey("givenName", base::Value(account.given_name));
  if (!account.account_image.IsEmpty()) {
    dictionary.SetKey(
        "avatarImage",
        base::Value(webui::GetBitmapDataUrl(account.account_image.AsBitmap())));
  }
  return dictionary;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

base::string16 GetEnterPassphraseBody(syncer::PassphraseType passphrase_type,
                                      base::Time passphrase_time) {
  DCHECK(syncer::IsExplicitPassphrase(passphrase_type));
  switch (passphrase_type) {
    case syncer::PassphraseType::kFrozenImplicitPassphrase:
      if (passphrase_time.is_null()) {
        return GetStringUTF16(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY);
      }
      return GetStringFUTF16(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
                             base::ASCIIToUTF16(chrome::kSyncErrorsHelpURL),
                             base::TimeFormatShortDate(passphrase_time));
    case syncer::PassphraseType::kCustomPassphrase:
      if (passphrase_time.is_null()) {
        return GetStringUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY);
      }
      return GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
                             base::ASCIIToUTF16(chrome::kSyncErrorsHelpURL),
                             base::TimeFormatShortDate(passphrase_time));
    case syncer::PassphraseType::kImplicitPassphrase:
    case syncer::PassphraseType::kKeystorePassphrase:
    case syncer::PassphraseType::kTrustedVaultPassphrase:
      break;
  }
  NOTREACHED();
  return base::string16();
}

base::string16 GetFullEncryptionBody(syncer::PassphraseType passphrase_type,
                                     base::Time passphrase_time) {
  DCHECK(syncer::IsExplicitPassphrase(passphrase_type));
  if (passphrase_time.is_null()) {
    return GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM);
  }
  switch (passphrase_type) {
    case syncer::PassphraseType::kFrozenImplicitPassphrase:
      return GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_GOOGLE_WITH_DATE,
                             base::TimeFormatShortDate(passphrase_time));
    case syncer::PassphraseType::kCustomPassphrase:
      return GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM_WITH_DATE,
                             base::TimeFormatShortDate(passphrase_time));
    case syncer::PassphraseType::kImplicitPassphrase:
    case syncer::PassphraseType::kKeystorePassphrase:
    case syncer::PassphraseType::kTrustedVaultPassphrase:
      break;
  }
  NOTREACHED();
  return base::string16();
}

}  // namespace

namespace settings {

// static
const char PeopleHandler::kSpinnerPageStatus[] = "spinner";
const char PeopleHandler::kConfigurePageStatus[] = "configure";
const char PeopleHandler::kTimeoutPageStatus[] = "timeout";
const char PeopleHandler::kDonePageStatus[] = "done";
const char PeopleHandler::kPassphraseFailedPageStatus[] = "passphraseFailed";

PeopleHandler::PeopleHandler(Profile* profile)
    : profile_(profile), configuring_sync_(false) {}

PeopleHandler::~PeopleHandler() {
  // Early exit if running unit tests (no actual WebUI is attached).
  if (!web_ui())
    return;

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
      "SyncPrefsDispatch",
      base::BindRepeating(&PeopleHandler::HandleSyncPrefsDispatch,
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

  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager)
    identity_manager_observer_.Add(identity_manager);

  // This is intentionally not using GetSyncService(), to go around the
  // Profile::IsSyncAllowed() check.
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service_observer_.Add(sync_service);
}

void PeopleHandler::OnJavascriptDisallowed() {
  profile_pref_registrar_.RemoveAll();
  identity_manager_observer_.RemoveAll();
  sync_service_observer_.RemoveAll();
}

#if !defined(OS_CHROMEOS)
void PeopleHandler::DisplayGaiaLogin(signin_metrics::AccessPoint access_point) {
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
  if (!browser)
    return;

  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser->profile());

  syncer::SyncService* service = GetSyncService();
  if (service && service->HasUnrecoverableError()) {
    // When the user has an unrecoverable error, they first have to sign out and
    // then sign in again.

    identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
        signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
        signin_metrics::SignoutDelete::IGNORE_METRIC);
  }

  // If the identity manager already has a primary account, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  if (identity_manager->HasPrimaryAccount()) {
    UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                              signin_metrics::HISTOGRAM_REAUTH_SHOWN,
                              signin_metrics::HISTOGRAM_REAUTH_MAX);

    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(browser->profile());
    DCHECK(error_controller->HasError());
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH, access_point, false);
  } else {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN, access_point, false);
  }
}
#endif

#if defined(OS_CHROMEOS)
void PeopleHandler::OnPinLoginAvailable(bool is_available) {
  FireWebUIListener("pin-login-available-changed", base::Value(is_available));
}
#endif

void PeopleHandler::OnDidClosePage(const base::ListValue* args) {
  // Don't mark setup as complete if "didAbort" is true, or if authentication
  // is still needed.
  if (!args->GetList()[0].GetBool() && !IsProfileAuthNeededOrHasErrors()) {
    MarkFirstSetupComplete();
  }

  CloseSyncSetup();
}

syncer::SyncService* PeopleHandler::GetSyncService() const {
  return profile_->IsSyncAllowed()
             ? ProfileSyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void PeopleHandler::HandleSetDatatypes(const base::ListValue* args) {
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

  service->GetUserSettings()->SetSelectedTypes(configuration.sync_everything,
                                               configuration.selected_types);

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

  ResolveJavascriptCallback(*callback_id, GetStoredAccountsList());
}

void PeopleHandler::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-updated", GetStoredAccountsList());
}

void PeopleHandler::OnExtendedAccountInfoRemoved(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-updated", GetStoredAccountsList());
}

base::Value PeopleHandler::GetStoredAccountsList() {
  base::Value accounts(base::Value::Type::LIST);
  const bool dice_enabled =
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_);

  if (dice_enabled) {
    // If dice is enabled, show all the accounts.
    for (auto const& account :
         signin_ui_util::GetAccountsForDicePromos(profile_)) {
      accounts.Append(GetAccountValue(account));
    }
  } else {
    // If dice is disabled (and unified consent enabled), show only the primary
    // account.
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    base::Optional<AccountInfo> primary_account_info =
        identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
            identity_manager->GetPrimaryAccountInfo());
    if (primary_account_info.has_value())
      accounts.Append(GetAccountValue(primary_account_info.value()));
  }

  return accounts;
}

void PeopleHandler::HandleStartSyncingWithEmail(const base::ListValue* args) {
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_));
  const base::Value* email;
  const base::Value* is_default_promo_account;
  CHECK(args->Get(0, &email));
  CHECK(args->Get(1, &is_default_promo_account));

  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());

  base::Optional<AccountInfo> maybe_account =
      IdentityManagerFactory::GetForProfile(profile_)
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              email->GetString());

  signin_ui_util::EnableSyncFromPromo(
      browser,
      maybe_account.has_value() ? maybe_account.value() : AccountInfo(),
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
      is_default_promo_account->GetBool());
}
#endif

void PeopleHandler::HandleSetEncryption(const base::ListValue* args) {
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

  // Don't allow "encrypt all" if the SyncService doesn't allow it.
  // The UI is hidden, but the user may have enabled it e.g. by fiddling with
  // the web inspector.
  if (!service->GetUserSettings()->IsEncryptEverythingAllowed()) {
    configuration.encrypt_all = false;
    configuration.set_new_passphrase = false;
  }

  // Note: Data encryption will not occur until configuration is complete
  // (when the PSS receives its CONFIGURE_DONE notification from the sync
  // engine), so the user still has a chance to cancel out of the operation
  // if (for example) some kind of passphrase error is encountered.
  if (configuration.encrypt_all)
    service->GetUserSettings()->EnableEncryptEverything();

  bool passphrase_failed = false;
  if (!configuration.passphrase.empty()) {
    // We call IsPassphraseRequired() here (instead of
    // IsPassphraseRequiredForPreferredDataTypes()) because the user may try to
    // enter a passphrase even though no encrypted data types are enabled.
    if (service->GetUserSettings()->IsPassphraseRequired()) {
      // If we have pending keys, try to decrypt them with the provided
      // passphrase. We track if this succeeds or fails because a failed
      // decryption should result in an error even if there aren't any encrypted
      // data types.
      passphrase_failed = !service->GetUserSettings()->SetDecryptionPassphrase(
          configuration.passphrase);
    } else {
      // OK, the user sent us a passphrase, but we don't have pending keys. So
      // it either means that the pending keys were resolved somehow since the
      // time the UI was displayed (re-encryption, pending passphrase change,
      // etc) or the user wants to re-encrypt.
      if (configuration.set_new_passphrase &&
          !service->GetUserSettings()->IsUsingSecondaryPassphrase()) {
        service->GetUserSettings()->SetEncryptionPassphrase(
            configuration.passphrase);
      }
    }
  }

  if (passphrase_failed ||
      service->GetUserSettings()->IsPassphraseRequiredForPreferredDataTypes()) {
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

  PushSyncPrefs();

  // Focus the web contents in case the location bar was focused before. This
  // makes sure that page elements for resolving sync errors can be focused.
  web_ui()->GetWebContents()->Focus();
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
  syncer::SyncService* service = GetSyncService();
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
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    if (identity_manager->HasPrimaryAccount()) {
      if (GetSyncService())
        syncer::RecordSyncEvent(syncer::STOP_FROM_OPTIONS);

      signin_metrics::SignoutDelete delete_metric =
          delete_profile ? signin_metrics::SignoutDelete::DELETED
                         : signin_metrics::SignoutDelete::KEEPING;

      identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
          signin::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
          signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, delete_metric);
    } else {
      DCHECK(!delete_profile)
          << "Deleting the profile should only be offered the user is syncing.";

      identity_manager->GetAccountsMutator()->RemoveAllAccounts(
          signin_metrics::SourceForRefreshTokenOperation::kSettings_Signout);
    }
  }

  if (delete_profile) {
    webui::DeleteProfileAtPath(profile_->GetPath(),
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }
}

void PeopleHandler::HandlePauseSync(const base::ListValue* args) {
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_));
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(identity_manager->HasPrimaryAccount());

  identity_manager->GetAccountsMutator()
      ->InvalidateRefreshTokenForPrimaryAccount(
          signin_metrics::SourceForRefreshTokenOperation::kSettings_PauseSync);
}
#endif

void PeopleHandler::HandleGetSyncStatus(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id, *GetSyncStatusDictionary());
}

void PeopleHandler::HandleSyncPrefsDispatch(const base::ListValue* args) {
  AllowJavascript();
  PushSyncPrefs();
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
        syncer::RecordSyncEvent(syncer::CANCEL_DURING_CONFIGURE);

        // If the user clicked "Cancel" while setting up sync, disable sync
        // because we don't want the sync engine to remain in the
        // first-setup-incomplete state.
        // Note: In order to disable sync across restarts on Chrome OS,
        // we must call StopAndClear(), which suppresses sync startup in
        // addition to disabling it.
        if (sync_service) {
          DVLOG(1) << "Sync setup aborted by user action";
          sync_service->StopAndClear();
#if !defined(OS_CHROMEOS)
          // Sign out the user on desktop Chrome if they click cancel during
          // initial setup.
          if (!sync_service->GetUserSettings()->IsFirstSetupComplete()) {
            IdentityManagerFactory::GetForProfile(profile_)
                ->GetPrimaryAccountMutator()
                ->ClearPrimaryAccount(
                    signin::PrimaryAccountMutator::ClearAccountsAction::
                        kDefault,
                    signin_metrics::ABORT_SIGNIN,
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

  if (IsSyncSubpage(web_contents->GetVisibleURL())) {
    sync_blocker_ = service->GetSetupInProgressHandle();
  }
}

void PeopleHandler::FocusUI() {
  WebContents* web_contents = web_ui()->GetWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void PeopleHandler::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  // After a primary account was set, the Sync setup will start soon. Grab a
  // SetupInProgressHandle right now to avoid a temporary "missing Sync
  // confirmation" error in the avatar menu. See crbug.com/928696.
  syncer::SyncService* service = GetSyncService();
  if (service && !sync_blocker_)
    sync_blocker_ = service->GetSetupInProgressHandle();

  UpdateSyncStatus();
}

void PeopleHandler::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  sync_blocker_.reset();
  UpdateSyncStatus();
}

void PeopleHandler::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncStatus();

  // When the SyncService changes its state, we should also push the updated
  // sync preferences.
  PushSyncPrefs();
}

void PeopleHandler::BeforeUnloadDialogCancelled() {
  // The before unload dialog is only shown during the first sync setup.
  DCHECK(IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount());
  syncer::SyncService* service = GetSyncService();
  DCHECK(service && service->IsSetupInProgress() &&
         !service->GetUserSettings()->IsFirstSetupComplete());

  base::RecordAction(
      base::UserMetricsAction("Signin_Signin_CancelAbortAdvancedSyncSettings"));
}

std::unique_ptr<base::DictionaryValue> PeopleHandler::GetSyncStatusDictionary()
    const {
  std::unique_ptr<base::DictionaryValue> sync_status(new base::DictionaryValue);
  if (profile_->IsGuestSession()) {
    // Cannot display signin status when running in guest mode on chromeos
    // because there is no IdentityManager.
    sync_status->SetBoolean("signinAllowed", false);
    return sync_status;
  }

  sync_status->SetBoolean("supervisedUser", profile_->IsSupervised());
  sync_status->SetBoolean("childUser", profile_->IsChild());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(identity_manager);

#if !defined(OS_CHROMEOS)
  // Signout is not allowed if the user has policy (crbug.com/172204).
  if (!signin_util::IsUserSignoutAllowedForProfile(profile_)) {
    std::string username = identity_manager->GetPrimaryAccountInfo().email;

    // If there is no one logged in or if the profile name is empty then the
    // domain name is empty. This happens in browser tests.
    if (!username.empty())
      sync_status->SetString("domain", gaia::ExtractDomainName(username));
  }
#endif

  // This is intentionally not using GetSyncService(), in order to access more
  // nuanced information, since GetSyncService() returns nullptr if anything
  // makes Profile::IsSyncAllowed() false.
  syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  bool disallowed_by_policy =
      service && service->HasDisableReason(
                     syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  sync_status->SetBoolean(
      "signinAllowed", profile_->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  sync_status->SetBoolean("syncSystemEnabled", (service != nullptr));
  sync_status->SetBoolean(
      "firstSetupInProgress",
      service && !disallowed_by_policy && service->IsSetupInProgress() &&
          !service->GetUserSettings()->IsFirstSetupComplete() &&
          identity_manager->HasPrimaryAccount());

  base::string16 status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;

  bool status_has_error =
      sync_ui_util::GetStatusLabels(profile_, &status_label, &link_label,
                                    &action_type) == sync_ui_util::SYNC_ERROR;
  sync_status->SetString("statusText", status_label);
  sync_status->SetString("statusActionText", link_label);
  sync_status->SetBoolean("hasError", status_has_error);
  sync_status->SetString("statusAction", GetSyncErrorAction(action_type));

  sync_status->SetBoolean("managed", disallowed_by_policy);
  sync_status->SetBoolean(
      "disabled", !service || disallowed_by_policy ||
                      !service->GetUserSettings()->IsSyncAllowedByPlatform());
  sync_status->SetBoolean("signedIn", identity_manager->HasPrimaryAccount());
  sync_status->SetString("signedInUsername",
                         signin_ui_util::GetAuthenticatedUsername(profile_));
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

  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service || !service->IsEngineInitialized()) {
    return;
  }

  configuring_sync_ = true;

  // Setup args for the sync configure screen:
  //   syncAllDataTypes: true if the user wants to sync everything
  //   <data_type>Registered: true if the associated data type is supported
  //   <data_type>Synced: true if the user wants to sync that specific data type
  //   paymentsIntegrationEnabled: true if the user wants Payments integration
  //   encryptionEnabled: true if sync supports encryption
  //   encryptAllData: true if user wants to encrypt all data (not just
  //       passwords)
  //   passphraseRequired: true if a passphrase is needed to start sync
  //
  base::DictionaryValue args;

  syncer::SyncUserSettings* sync_user_settings = service->GetUserSettings();
  // Tell the UI layer which data types are registered/enabled by the user.
  const syncer::UserSelectableTypeSet registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  const syncer::UserSelectableTypeSet selected_types =
      sync_user_settings->GetSelectedTypes();
  const syncer::UserSelectableTypeSet enforced_types =
      sync_user_settings->GetForcedTypes();
  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    const std::string type_name = syncer::GetUserSelectableTypeName(type);
    args.SetBoolean(type_name + "Registered", registered_types.Has(type));
    args.SetBoolean(type_name + "Synced", selected_types.Has(type));
    args.SetBoolean(type_name + "Enforced", enforced_types.Has(type));
  }
  args.SetBoolean("syncAllDataTypes",
                  sync_user_settings->IsSyncEverythingEnabled());
  args.SetBoolean(
      "paymentsIntegrationEnabled",
      autofill::prefs::IsPaymentsIntegrationEnabled(profile_->GetPrefs()));
  args.SetBoolean("encryptAllData",
                  sync_user_settings->IsEncryptEverythingEnabled());
  args.SetBoolean("encryptAllDataAllowed",
                  sync_user_settings->IsEncryptEverythingAllowed());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForPreferredDataTypes(), because we want to show the
  // passphrase UI even if no encrypted data types are enabled.
  args.SetBoolean("passphraseRequired",
                  sync_user_settings->IsPassphraseRequired());

  syncer::PassphraseType passphrase_type =
      sync_user_settings->GetPassphraseType();
  if (syncer::IsExplicitPassphrase(passphrase_type)) {
    base::Time passphrase_time =
        sync_user_settings->GetExplicitPassphraseTime();
    args.SetString("enterPassphraseBody",
                   GetEnterPassphraseBody(passphrase_type, passphrase_time));
    args.SetString("fullEncryptionBody",
                   GetFullEncryptionBody(passphrase_type, passphrase_time));
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

bool PeopleHandler::IsProfileAuthNeededOrHasErrors() {
  return !IdentityManagerFactory::GetForProfile(profile_)
              ->HasPrimaryAccount() ||
         SigninErrorControllerFactory::GetForProfile(profile_)->HasError();
}

}  // namespace settings
