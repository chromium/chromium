// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_prefs.h"

#include <utility>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"

namespace syncer {

namespace {

// Historic artifact for payment methods, migrated since 2023/07 to
// prefs::internal::kSyncPayments to make the name consistent with other
// user-selectable types.
constexpr char kObsoleteAutofillWalletImportEnabled[] =
    "autofill.wallet_import_enabled";

// Boolean representing whether kObsoleteAutofillWalletImportEnabled was
// migrated to the new pref representing a UserSelectableType. Should be cleaned
// up together with the migration code (after 2024-07).
constexpr char kObsoleteAutofillWalletImportEnabledMigrated[] =
    "sync.autofill_wallet_import_enabled_migrated";

// State of the migration done by MaybeMigrateCustomPassphrasePref().
constexpr char kSyncEncryptionBootstrapTokenPerAccountMigrationDone[] =
    "sync.encryption_bootstrap_token_per_account_migration_done";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Pref to record if the one-off MaybeMigrateAutofillToPerAccountPref()
// migration ran.
constexpr char kAutofillPerAccountPrefMigrationDone[] =
    "sync.passwords_per_account_pref_migration_done";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

constexpr int kNotMigrated = 0;
constexpr int kMigratedPart1ButNot2 = 1;
constexpr int kMigratedPart2AndFullyDone = 2;

// Encodes a protobuf instance of type
// sync_pb::TrustedVaultAutoUpgradeExperimentGroup in a way that can be safely
// stored in prefs, i.e. using base64 encoding.
std::string EncodeTrustedVaultAutoUpgradeExperimentGroupToString(
    const sync_pb::TrustedVaultAutoUpgradeExperimentGroup& group) {
  return base::Base64Encode(group.SerializeAsString());
}

// Does the opposite of EncodeTrustedVaultAutoUpgradeExperimentGroupToString(),
// i.e. transforms from a string representation to a protobuf instance.
sync_pb::TrustedVaultAutoUpgradeExperimentGroup
DecodeTrustedVaultAutoUpgradeExperimentGroupFromString(
    const std::string& encoded_group) {
  sync_pb::TrustedVaultAutoUpgradeExperimentGroup proto;
  std::string serialized_proto;
  if (!base::Base64Decode(encoded_group, &serialized_proto)) {
    return proto;
  }
  proto.ParseFromString(serialized_proto);
  return proto;
}

}  // namespace

SyncPrefObserver::~SyncPrefObserver() = default;

SyncPrefs::SyncPrefs(PrefService* pref_service)
    : pref_service_(pref_service),
      local_sync_enabled_(
          pref_service_->GetBoolean(prefs::kEnableLocalSyncBackend)) {
  DCHECK(pref_service);
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::internal::kSyncManaged, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncManagedPrefChanged,
                          base::Unretained(this)));

  // Observe changes to all of the prefs that may influence the selected types.
  pref_change_registrar_.Init(pref_service_);
  // The individual data type prefs are used for syncing users, as well as for
  // enterprise policy.
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    pref_change_registrar_.Add(
        GetPrefNameForType(type),
        base::BindRepeating(&SyncPrefs::OnSelectedTypesPrefChanged,
                            base::Unretained(this)));
  }
  // The "sync everything" pref is used for syncing users.
  pref_change_registrar_.Add(
      prefs::internal::kSyncKeepEverythingSynced,
      base::BindRepeating(&SyncPrefs::OnSelectedTypesPrefChanged,
                          base::Unretained(this)));
  // The per-account data types dictionary pref is used for signed-in
  // non-syncing users.
  pref_change_registrar_.Add(
      prefs::internal::kSelectedTypesPerAccount,
      base::BindRepeating(&SyncPrefs::OnSelectedTypesPrefChanged,
                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On ChromeOS-Lacros, syncing of apps is determined by a special
  // Ash-controlled pref.
  pref_change_registrar_.Add(
      prefs::internal::kSyncAppsEnabledByOs,
      base::BindRepeating(&SyncPrefs::OnSelectedTypesPrefChanged,
                          base::Unretained(this)));
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  pref_initial_sync_feature_setup_complete_.Init(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, pref_service_,
      base::BindRepeating(&SyncPrefs::OnFirstSetupCompletePrefChange,
                          base::Unretained(this)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

SyncPrefs::~SyncPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void SyncPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Actual user-controlled preferences.
  registry->RegisterBooleanPref(prefs::internal::kSyncKeepEverythingSynced,
                                true);
  registry->RegisterDictionaryPref(prefs::internal::kSelectedTypesPerAccount);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    RegisterTypeSelectedPref(registry, type);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::internal::kSyncDisabledViaDashboard,
                                false);
  registry->RegisterBooleanPref(prefs::internal::kSyncAllOsTypes, true);
  registry->RegisterBooleanPref(prefs::internal::kSyncOsApps, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncOsPreferences, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncWifiConfigurations,
                                false);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  registry->RegisterBooleanPref(prefs::internal::kSyncAppsEnabledByOs, false);
#endif

  registry->RegisterBooleanPref(kObsoleteAutofillWalletImportEnabledMigrated,
                                false);
  registry->RegisterIntegerPref(prefs::internal::kSyncToSigninMigrationState,
                                kNotMigrated);
  registry->RegisterBooleanPref(
      prefs::internal::kMigrateReadingListFromLocalToAccount, false);

  // The passphrase type, determined upon the first engine initialization.
  registry->RegisterIntegerPref(
      prefs::internal::kSyncCachedPassphraseType,
      sync_pb::NigoriSpecifics_PassphraseType_UNKNOWN);
  // The user's TrustedVaultAutoUpgradeExperimentGroup, determined the first
  // time the engine is successfully initialized.
  registry->RegisterStringPref(
      prefs::internal::kSyncCachedTrustedVaultAutoUpgradeExperimentGroup, "");
  // The encryption bootstrap token represents a user-entered passphrase.
  registry->RegisterStringPref(prefs::internal::kSyncEncryptionBootstrapToken,
                               std::string());

  // The encryption bootstrap token represents a user-entered passphrase per
  // account.
  registry->RegisterDictionaryPref(
      prefs::internal::kSyncEncryptionBootstrapTokenPerAccount);
  // For migration only, tracks if the EncryptionBootstrapToken is migrated.
  registry->RegisterBooleanPref(
      kSyncEncryptionBootstrapTokenPerAccountMigrationDone, false);

  registry->RegisterBooleanPref(prefs::internal::kSyncManaged, false);
  registry->RegisterIntegerPref(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion, 0);
  registry->RegisterBooleanPref(prefs::kEnableLocalSyncBackend, false);
  registry->RegisterFilePathPref(prefs::kLocalSyncBackendDir, base::FilePath());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(kAutofillPerAccountPrefMigrationDone, false);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  registry->RegisterTimePref(
      prefs::internal::kFirstTimeTriedToMigrateSyncFeaturePausedToSignin,
      base::Time());
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::internal::kWipedWebAPkDataForMigration,
                                false);
#endif  // BUILDFLAG(IS_ANDROID)

  SyncFeatureStatusForMigrationsRecorder::RegisterProfilePrefs(registry);

  // Obsolete prefs (registered for migrations only).
  registry->RegisterBooleanPref(kObsoleteAutofillWalletImportEnabled, true);
}

void SyncPrefs::AddObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

bool SyncPrefs::IsInitialSyncFeatureSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return pref_service_->GetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool SyncPrefs::IsExplicitBrowserSignin() const {
  return pref_service_->GetBoolean(::prefs::kExplicitBrowserSignin);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SyncPrefs::SetInitialSyncFeatureSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
}

void SyncPrefs::ClearInitialSyncFeatureSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncKeepEverythingSynced);
}

UserSelectableTypeSet SyncPrefs::GetSelectedTypesForAccount(
    const signin::GaiaIdHash& gaia_id_hash) const {
  UserSelectableTypeSet selected_types;

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    if (!IsTypeSupportedInTransportMode(type)) {
      continue;
    }
    const char* pref_name = GetPrefNameForType(type);
    DCHECK(pref_name);
    bool type_enabled = false;
    if (IsTypeManagedByPolicy(type) || IsTypeManagedByCustodian(type)) {
      type_enabled = pref_service_->GetBoolean(pref_name);
    } else {
      const base::Value* pref_value = GetAccountKeyedPrefDictEntry(
          pref_service_, prefs::internal::kSelectedTypesPerAccount,
          gaia_id_hash, pref_name);
      if (pref_value && pref_value->is_bool()) {
        type_enabled = pref_value->GetBool();
      } else if (type == UserSelectableType::kHistory ||
                 type == UserSelectableType::kTabs ||
                 type == UserSelectableType::kSavedTabGroups ||
                 type == UserSelectableType::kSharedTabGroupData) {
        // History, Tabs, Saved Tab Groups and and Shared Tab Group Data are
        // disabled by default.
        type_enabled = false;
      } else if (type == UserSelectableType::kPasswords ||
                 type == UserSelectableType::kAutofill) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
        type_enabled = true;
#else
        // kPasswords and kAutofill are only on by default if there was an
        // explicit sign in recorded.
        // Otherwise:
        // - kPasswords requires a dedicated opt-in.
        // - kAutofill cannot be enabled.
        // Note: If this changes, also update the migration logic in
        // MigrateGlobalDataTypePrefsToAccount().
        type_enabled =
            pref_service_->GetBoolean(::prefs::kExplicitBrowserSignin);
#endif
      } else if (type == UserSelectableType::kBookmarks ||
                 type == UserSelectableType::kReadingList) {
        type_enabled = true;
        // Consider kBookmarks and kReadingList off by default until
        // `kReplaceSyncPromosWithSignInPromos` is enabled. For existing clients
        // at the time the feature transitions from disabled to enabled, the
        // state at the time is captured as explicit value in
        // `MaybeMigratePrefsForSyncToSigninPart1()`.
        if (!base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos) &&
            !base::FeatureList::IsEnabled(
                kEnableBookmarksSelectedTypeOnSigninForTesting)) {
          type_enabled = false;
        }
      } else if (type == UserSelectableType::kExtensions) {
        // Extensions require an explicit sign in.
        type_enabled =
            pref_service_->GetBoolean(::prefs::kExplicitBrowserSignin);
      } else {
        // All other types are always enabled by default.
        type_enabled = true;
      }
    }
    if (type_enabled) {
      selected_types.Put(type);
    }
  }

  if (!password_sync_allowed_) {
    selected_types.Remove(UserSelectableType::kPasswords);
  }

  return selected_types;
}

UserSelectableTypeSet SyncPrefs::GetSelectedTypesForSyncingUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UserSelectableTypeSet selected_types;

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    const char* pref_name = GetPrefNameForType(type);
    DCHECK(pref_name);
    if (pref_service_->GetBoolean(pref_name) ||
        (!IsTypeManagedByPolicy(type) && !IsTypeManagedByCustodian(type) &&
         pref_service_->GetBoolean(
             prefs::internal::kSyncKeepEverythingSynced))) {
      // In full-sync mode, the "sync everything" bit is honored. If it's
      // true, all types are considered selected, irrespective of their
      // individual prefs.
      selected_types.Put(type);
    }
  }

  if (!password_sync_allowed_) {
    selected_types.Remove(UserSelectableType::kPasswords);
  }

  return selected_types;
}

bool SyncPrefs::IsTypeManagedByPolicy(UserSelectableType type) const {
  const char* pref_name = GetPrefNameForType(type);
  CHECK(pref_name);
  return pref_service_->IsManagedPreference(pref_name);
}

bool SyncPrefs::IsTypeManagedByCustodian(UserSelectableType type) const {
  const char* pref_name = GetPrefNameForType(type);
  CHECK(pref_name);
  return pref_service_->IsPreferenceManagedByCustodian(pref_name);
}

bool SyncPrefs::DoesTypeHaveDefaultValueForAccount(
    const UserSelectableType type,
    const signin::GaiaIdHash& gaia_id_hash) {
  const char* pref_name = GetPrefNameForType(type);
  DCHECK(pref_name);

  const base::Value* value = GetAccountKeyedPrefDictEntry(
      pref_service_, prefs::internal::kSelectedTypesPerAccount, gaia_id_hash,
      pref_name);

  return !value;
}

bool SyncPrefs::IsTypeDisabledByUserForAccount(
    const UserSelectableType type,
    const signin::GaiaIdHash& gaia_id_hash) {
  const char* pref_name = GetPrefNameForType(type);
  DCHECK(pref_name);

  const base::Value* value = GetAccountKeyedPrefDictEntry(
      pref_service_, prefs::internal::kSelectedTypesPerAccount, gaia_id_hash,
      pref_name);

  if (value && value->is_bool()) {
    return !value->GetBool();
  }
  return false;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
int SyncPrefs::GetNumberOfAccountsWithPasswordsSelected() const {
  int n_accounts = 0;
  for (auto [serialized_gaia_id_hash, selected_types] :
       pref_service_->GetDict(prefs::internal::kSelectedTypesPerAccount)) {
    // `selected_types` should be a dict but doesn't hurt to check and be safe.
    bool enabled =
        selected_types.is_dict() &&
        selected_types.GetDict()
            .FindBool(GetPrefNameForType(UserSelectableType::kPasswords))
            .value_or(false);
    if (enabled) {
      n_accounts++;
    }
  }
  return n_accounts;
}
#endif

void SyncPrefs::SetSelectedTypesForSyncingUser(
    bool keep_everything_synced,
    UserSelectableTypeSet registered_types,
    UserSelectableTypeSet selected_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  {
    // Prevent OnSelectedTypesPrefChanged() from notifying observers about each
    // of the type changes individually.
    base::AutoReset<bool> batch_update(&batch_updating_selected_types_, true);

    pref_service_->SetBoolean(prefs::internal::kSyncKeepEverythingSynced,
                              keep_everything_synced);

    for (UserSelectableType type : registered_types) {
      const char* pref_name = GetPrefNameForType(type);
      pref_service_->SetBoolean(pref_name, selected_types.Has(type));
    }
  }

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnSelectedTypesPrefChange();
  }
}

void SyncPrefs::SetSelectedTypeForAccount(
    UserSelectableType type,
    bool is_type_on,
    const signin::GaiaIdHash& gaia_id_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetAccountKeyedPrefDictEntry(
      pref_service_, prefs::internal::kSelectedTypesPerAccount, gaia_id_hash,
      GetPrefNameForType(type), base::Value(is_type_on));
}

void SyncPrefs::KeepAccountSettingsPrefsOnlyForUsers(
    const std::vector<signin::GaiaIdHash>& available_gaia_ids) {
  KeepAccountKeyedPrefValuesOnlyForUsers(
      pref_service_, prefs::internal::kSelectedTypesPerAccount,
      available_gaia_ids);
  KeepAccountKeyedPrefValuesOnlyForUsers(
      pref_service_, prefs::internal::kSyncEncryptionBootstrapTokenPerAccount,
      available_gaia_ids);

  // TODO(crbug.com/368409110): This is not the right place for clearing
  // transport-data-related prefs - ideally there'd be an observer API for
  // "accounts on this device".
  SyncTransportDataPrefs::KeepAccountSettingsPrefsOnlyForUsers(
      pref_service_, available_gaia_ids);

  // TODO(crbug.com/368409110): This is *absolutely* not the right place for
  // clearing not-sync-related prefs. Move this elsewhere once signin code
  // provides an observer API for "accounts on this device".
  tab_groups::prefs::KeepAccountSettingsPrefsOnlyForUsers(pref_service_,
                                                          available_gaia_ids);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SyncPrefs::IsSyncFeatureDisabledViaDashboard() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncDisabledViaDashboard);
}

void SyncPrefs::SetSyncFeatureDisabledViaDashboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::internal::kSyncDisabledViaDashboard, true);
}

void SyncPrefs::ClearSyncFeatureDisabledViaDashboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::internal::kSyncDisabledViaDashboard);
}

bool SyncPrefs::IsSyncAllOsTypesEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncAllOsTypes);
}

UserSelectableOsTypeSet SyncPrefs::GetSelectedOsTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UserSelectableOsTypeSet selected_types;
  const bool sync_all_os_types = IsSyncAllOsTypesEnabled();
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    const char* pref_name = GetPrefNameForOsType(type);
    DCHECK(pref_name);
    // If the type is managed, |sync_all_os_types| is ignored for this type.
    if (pref_service_->GetBoolean(pref_name) ||
        (sync_all_os_types && !IsOsTypeManagedByPolicy(type))) {
      selected_types.Put(type);
    }
  }
  return selected_types;
}

bool SyncPrefs::IsOsTypeManagedByPolicy(UserSelectableOsType type) const {
  const char* pref_name = GetPrefNameForOsType(type);
  CHECK(pref_name);
  return pref_service_->IsManagedPreference(pref_name);
}

void SyncPrefs::SetSelectedOsTypes(bool sync_all_os_types,
                                   UserSelectableOsTypeSet registered_types,
                                   UserSelectableOsTypeSet selected_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    // Prevent OnSelectedTypesPrefChanged() from notifying about each of the
    // type changes individually.
    base::AutoReset<bool> batch_update(&batch_updating_selected_types_, true);

    pref_service_->SetBoolean(prefs::internal::kSyncAllOsTypes,
                              sync_all_os_types);
    for (UserSelectableOsType type : registered_types) {
      const char* pref_name = GetPrefNameForOsType(type);
      DCHECK(pref_name);
      pref_service_->SetBoolean(pref_name, selected_types.Has(type));
    }
  }
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnSelectedTypesPrefChange();
  }
}

// static
const char* SyncPrefs::GetPrefNameForOsTypeForTesting(
    UserSelectableOsType type) {
  return GetPrefNameForOsType(type);
}

// static
const char* SyncPrefs::GetPrefNameForOsType(UserSelectableOsType type) {
  switch (type) {
    case UserSelectableOsType::kOsApps:
      return prefs::internal::kSyncOsApps;
    case UserSelectableOsType::kOsPreferences:
      return prefs::internal::kSyncOsPreferences;
    case UserSelectableOsType::kOsWifiConfigurations:
      return prefs::internal::kSyncWifiConfigurations;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// static
void SyncPrefs::SetOsTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                          UserSelectableOsType type) {
  const char* pref_name = syncer::SyncPrefs::GetPrefNameForOsType(type);
  CHECK(pref_name);
  policy_prefs->SetValue(pref_name, base::Value(false));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool SyncPrefs::IsAppsSyncEnabledByOs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncAppsEnabledByOs);
}

void SyncPrefs::SetAppsSyncEnabledByOs(bool apps_sync_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::internal::kSyncAppsEnabledByOs,
                            apps_sync_enabled);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

bool SyncPrefs::IsSyncClientDisabledByPolicy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncManaged);
}

std::optional<PassphraseType> SyncPrefs::GetCachedPassphraseType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ProtoPassphraseInt32ToEnum(
      pref_service_->GetInteger(prefs::internal::kSyncCachedPassphraseType));
}

void SyncPrefs::SetCachedPassphraseType(PassphraseType passphrase_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInteger(prefs::internal::kSyncCachedPassphraseType,
                            EnumPassphraseTypeToProto(passphrase_type));
}

void SyncPrefs::ClearCachedPassphraseType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::internal::kSyncCachedPassphraseType);
}

std::optional<sync_pb::TrustedVaultAutoUpgradeExperimentGroup>
SyncPrefs::GetCachedTrustedVaultAutoUpgradeExperimentGroup() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& encoded_group = pref_service_->GetString(
      prefs::internal::kSyncCachedTrustedVaultAutoUpgradeExperimentGroup);
  if (encoded_group.empty()) {
    return std::nullopt;
  }
  return DecodeTrustedVaultAutoUpgradeExperimentGroupFromString(encoded_group);
}

void SyncPrefs::SetCachedTrustedVaultAutoUpgradeExperimentGroup(
    const sync_pb::TrustedVaultAutoUpgradeExperimentGroup& group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(
      prefs::internal::kSyncCachedTrustedVaultAutoUpgradeExperimentGroup,
      EncodeTrustedVaultAutoUpgradeExperimentGroupToString(group));
}

void SyncPrefs::ClearCachedTrustedVaultAutoUpgradeExperimentGroup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(
      prefs::internal::kSyncCachedTrustedVaultAutoUpgradeExperimentGroup);
}

void SyncPrefs::ClearAllEncryptionBootstrapTokens() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::internal::kSyncEncryptionBootstrapToken);
  if (IsInitialSyncFeatureSetupComplete()) {
    // When Sync-the-feature gets turned off, the user's encryption bootstrap
    // token should be cleared. However, at this point it's hard to determine
    // the right account, since the user is already signed out. For simplicity,
    // just clear all existing bootstrap tokens (in practice, there will almost
    // always be at most one anyway).
    KeepAccountKeyedPrefValuesOnlyForUsers(
        pref_service_, prefs::internal::kSyncEncryptionBootstrapTokenPerAccount,
        {});
  }
}

std::string SyncPrefs::GetEncryptionBootstrapTokenForAccount(
    const signin::GaiaIdHash& gaia_id_hash) const {
  CHECK(gaia_id_hash.IsValid());
  const std::string* account_passphrase =
      pref_service_
          ->GetDict(prefs::internal::kSyncEncryptionBootstrapTokenPerAccount)
          .FindString(gaia_id_hash.ToBase64());
  return account_passphrase ? *account_passphrase : std::string();
}

void SyncPrefs::SetEncryptionBootstrapTokenForAccount(
    const std::string& token,
    const signin::GaiaIdHash& gaia_id_hash) {
  CHECK(gaia_id_hash.IsValid());
  SetAccountKeyedPrefValue(
      pref_service_, prefs::internal::kSyncEncryptionBootstrapTokenPerAccount,
      gaia_id_hash, base::Value(token));
}

void SyncPrefs::ClearEncryptionBootstrapTokenForAccount(
    const signin::GaiaIdHash& gaia_id_hash) {
  CHECK(gaia_id_hash.IsValid());
  ClearAccountKeyedPrefValue(
      pref_service_, prefs::internal::kSyncEncryptionBootstrapTokenPerAccount,
      gaia_id_hash);
}

// static
const char* SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType type) {
  return GetPrefNameForType(type);
}

// static
const char* SyncPrefs::GetPrefNameForType(UserSelectableType type) {
  switch (type) {
    case UserSelectableType::kBookmarks:
      return prefs::internal::kSyncBookmarks;
    case UserSelectableType::kPreferences:
      return prefs::internal::kSyncPreferences;
    case UserSelectableType::kPasswords:
      return prefs::internal::kSyncPasswords;
    case UserSelectableType::kAutofill:
      return prefs::internal::kSyncAutofill;
    case UserSelectableType::kThemes:
      return prefs::internal::kSyncThemes;
    case UserSelectableType::kHistory:
      return prefs::internal::kSyncHistory;
    case UserSelectableType::kExtensions:
      return prefs::internal::kSyncExtensions;
    case UserSelectableType::kApps:
      return prefs::internal::kSyncApps;
    case UserSelectableType::kReadingList:
      return prefs::internal::kSyncReadingList;
    case UserSelectableType::kTabs:
      return prefs::internal::kSyncTabs;
    case UserSelectableType::kSavedTabGroups:
      return prefs::internal::kSyncSavedTabGroups;
    case UserSelectableType::kSharedTabGroupData:
      return prefs::internal::kSyncSharedTabGroupData;
    case UserSelectableType::kPayments:
      return prefs::internal::kSyncPayments;
    case UserSelectableType::kProductComparison:
      return prefs::internal::kSyncProductComparison;
    case UserSelectableType::kCookies:
      return prefs::internal::kSyncCookies;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// static
void SyncPrefs::SetTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                        UserSelectableType type) {
  const char* pref_name = syncer::SyncPrefs::GetPrefNameForType(type);
  CHECK(pref_name);
  CHECK(policy_prefs);
  policy_prefs->SetValue(pref_name, base::Value(false));
}

// static void
void SyncPrefs::SetTypeDisabledByCustodian(PrefValueMap* supervised_user_prefs,
                                           UserSelectableType type) {
  const char* pref_name = syncer::SyncPrefs::GetPrefNameForType(type);
  CHECK(pref_name);
  CHECK(supervised_user_prefs);
  supervised_user_prefs->SetValue(pref_name, base::Value(false));
}

// static
bool SyncPrefs::IsTypeSupportedInTransportMode(UserSelectableType type) {
  // Not all types are supported in transport mode, and many require one or more
  // Features to be enabled.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return base::FeatureList::IsEnabled(kSyncEnableBookmarksInTransportMode);
    case UserSelectableType::kReadingList:
      return syncer::IsReadingListAccountStorageEnabled();
    case UserSelectableType::kPreferences:
      return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos) &&
             base::FeatureList::IsEnabled(kEnablePreferencesAccountStorage);
    case UserSelectableType::kPasswords:
      // WARNING: This should actually be checking
      // password_manager::features_util::CanCreateAccountStore() too, otherwise
      // a crash can happen. But a) it would require a cyclic dependency, and
      // b) by the time kEnablePasswordsAccountStorageForNonSyncingUsers is
      // rolled out on Android, CanCreateAccountStore() should always return
      // true (or at least it can be some trivial GmsCore version check and live
      // in components/sync/).
      return base::FeatureList::IsEnabled(
          kEnablePasswordsAccountStorageForNonSyncingUsers);
    case UserSelectableType::kAutofill:
      return base::FeatureList::IsEnabled(
          kSyncEnableContactInfoDataTypeInTransportMode);
    case UserSelectableType::kPayments:
      // Always supported, since AUTOFILL_WALLET_DATA is supported in
      // transport mode everywhere.
      return true;
    case UserSelectableType::kHistory:
    case UserSelectableType::kTabs:
      return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos);
    case UserSelectableType::kProductComparison:
      return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos);
    case syncer::UserSelectableType::kSharedTabGroupData:
      return base::FeatureList::IsEnabled(
          kSyncSharedTabGroupDataInTransportMode);
    case UserSelectableType::kSavedTabGroups:
      return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos);
    case UserSelectableType::kExtensions:
      return base::FeatureList::IsEnabled(kSyncEnableExtensionsInTransportMode);
    case UserSelectableType::kApps:
    case UserSelectableType::kThemes:
    case UserSelectableType::kCookies:
      // These types are not supported in transport mode yet.
      return false;
  }
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnSyncManagedPrefChange(*pref_sync_managed_);
  }
}

void SyncPrefs::OnSelectedTypesPrefChanged(const std::string& pref_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there is a batch update of selected-types prefs ongoing, don't notify
  // observers - the call site will take care of it.
  if (batch_updating_selected_types_) {
    return;
  }

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnSelectedTypesPrefChange();
  }
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SyncPrefs::OnFirstSetupCompletePrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnFirstSetupCompletePrefChange(
        *pref_initial_sync_feature_setup_complete_);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// static
void SyncPrefs::RegisterTypeSelectedPref(PrefRegistrySimple* registry,
                                         UserSelectableType type) {
  const char* pref_name = GetPrefNameForType(type);
  DCHECK(pref_name);
  registry->RegisterBooleanPref(pref_name, false);
}

bool SyncPrefs::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

int SyncPrefs::GetPassphrasePromptMutedProductVersion() const {
  return pref_service_->GetInteger(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion);
}

void SyncPrefs::SetPassphrasePromptMutedProductVersion(int major_version) {
  pref_service_->SetInteger(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion, major_version);
}

void SyncPrefs::ClearPassphrasePromptMutedProductVersion() {
  pref_service_->ClearPref(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion);
}

bool SyncPrefs::MaybeMigratePrefsForSyncToSigninPart1(
    SyncAccountState account_state,
    const signin::GaiaIdHash& gaia_id_hash) {
  if (!base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos)) {
    // Ensure that the migration runs again when the feature gets enabled.
    pref_service_->ClearPref(prefs::internal::kSyncToSigninMigrationState);
    return false;
  }

  // Don't migrate again if this profile was previously migrated.
  if (pref_service_->GetInteger(prefs::internal::kSyncToSigninMigrationState) !=
      kNotMigrated) {
    return false;
  }

  if (IsLocalSyncEnabled()) {
    // Special case for local sync: There isn't necessarily a signed-in user
    // (even if the SyncAccountState is kSyncing), so just mark the migration as
    // done.
    pref_service_->SetInteger(prefs::internal::kSyncToSigninMigrationState,
                              kMigratedPart2AndFullyDone);
    return false;
  }

  switch (account_state) {
    case SyncAccountState::kNotSignedIn:
    case SyncAccountState::kSyncing: {
      // Nothing to migrate for signed-out or syncing users. Also make sure the
      // second part of the migration does *not* run if a signed-out user does
      // later sign in / turn on sync.
      pref_service_->SetInteger(prefs::internal::kSyncToSigninMigrationState,
                                kMigratedPart2AndFullyDone);
      return false;
    }
    case SyncAccountState::kSignedInNotSyncing: {
      pref_service_->SetInteger(prefs::internal::kSyncToSigninMigrationState,
                                kMigratedPart1ButNot2);
      CHECK(gaia_id_hash.IsValid());
      ScopedDictPrefUpdate update_selected_types_dict(
          pref_service_, prefs::internal::kSelectedTypesPerAccount);
      base::Value::Dict* account_settings =
          update_selected_types_dict->EnsureDict(gaia_id_hash.ToBase64());

      // Settings aka preferences always starts out "off" for existing
      // signed-in non-syncing users.
      account_settings->Set(
          GetPrefNameForType(UserSelectableType::kPreferences), false);

      // Bookmarks and reading list remain enabled only if the user previously
      // explicitly opted in, which is represented in the regular account-keyed
      // prefs. However, the default value for new sign-ins changes with
      // `kReplaceSyncPromosWithSignInPromos`, so it is important to grab a
      // snapshot now during migration.
      for (UserSelectableType type :
           {UserSelectableType::kBookmarks, UserSelectableType::kReadingList}) {
        const char* pref_name = GetPrefNameForType(type);
        DCHECK(pref_name);

        const base::Value* value = account_settings->Find(pref_name);
        const bool is_type_on = value && value->is_bool() && value->GetBool();

        // Setting the value explicitly is important to convert absence to
        // false, so it doesn't use the default, which is enabled after
        // `kReplaceSyncPromosWithSignInPromos`.
        account_settings->Set(pref_name, is_type_on);
      }

      return true;
    }
  }
}

bool SyncPrefs::MaybeMigratePrefsForSyncToSigninPart2(
    const signin::GaiaIdHash& gaia_id_hash,
    bool is_using_explicit_passphrase) {
  // The migration pref shouldn't be set if the feature is disabled, but if it
  // somehow happened, do *not* run the migration, and clear the pref so that
  // the migration will get triggered again once the feature gets enabled again.
  if (!base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos)) {
    pref_service_->ClearPref(prefs::internal::kSyncToSigninMigrationState);
    return false;
  }

  // Only run part 2 of the migration if part 1 has run but part 2 hasn't yet.
  // This ensures that it only runs once.
  if (pref_service_->GetInteger(prefs::internal::kSyncToSigninMigrationState) !=
      kMigratedPart1ButNot2) {
    return false;
  }
  pref_service_->SetInteger(prefs::internal::kSyncToSigninMigrationState,
                            kMigratedPart2AndFullyDone);

  // The actual migration: For explicit-passphrase users, addresses sync gets
  // disabled by default.
  if (is_using_explicit_passphrase && !IsLocalSyncEnabled()) {
    CHECK(gaia_id_hash.IsValid());
    SetAccountKeyedPrefDictEntry(
        pref_service_, prefs::internal::kSelectedTypesPerAccount, gaia_id_hash,
        GetPrefNameForType(UserSelectableType::kAutofill), base::Value(false));
    return true;
  }
  return false;
}

void SyncPrefs::MaybeMigrateCustomPassphrasePref(
    const signin::GaiaIdHash& gaia_id_hash) {
  if (pref_service_->GetBoolean(
          kSyncEncryptionBootstrapTokenPerAccountMigrationDone)) {
    return;
  }
  pref_service_->SetBoolean(
      kSyncEncryptionBootstrapTokenPerAccountMigrationDone, true);

  if (gaia_id_hash == signin::GaiaIdHash::FromGaiaId("")) {
    // Do not migrate if gaia_id is empty; no signed in user.
    return;
  }

  CHECK(gaia_id_hash.IsValid());
  const std::string& token =
      pref_service_->GetString(prefs::internal::kSyncEncryptionBootstrapToken);
  if (token.empty()) {
    // No custom passphrase is used, or it is not set.
    return;
  }
  SetAccountKeyedPrefValue(
      pref_service_, prefs::internal::kSyncEncryptionBootstrapTokenPerAccount,
      gaia_id_hash, base::Value(token));
  return;
}

// static
void SyncPrefs::MigrateAutofillWalletImportEnabledPref(
    PrefService* pref_service) {
  if (pref_service->GetBoolean(kObsoleteAutofillWalletImportEnabledMigrated)) {
    // Migration already happened; nothing else needed.
    return;
  }

  const base::Value* autofill_wallet_import_enabled =
      pref_service->GetUserPrefValue(kObsoleteAutofillWalletImportEnabled);

  if (autofill_wallet_import_enabled != nullptr) {
    // If the previous pref was populated explicitly, carry over the value.
    pref_service->SetBoolean(prefs::internal::kSyncPayments,
                             autofill_wallet_import_enabled->GetBool());
  } else if (pref_service->GetBoolean(
                 prefs::internal::kSyncKeepEverythingSynced)) {
    // The old pref isn't explicitly set (defaults to true) and sync-everything
    // is on: in this case there is no need to explicitly set individual
    // UserSelectableType to true.
  } else {
    // There is a special case for very old profiles, created before 2019 (i.e.
    // before https://codereview.chromium.org/2068653003 and similar code
    // changes). In older versions of the UI, it was possible to set
    // kSyncKeepEverythingSynced to false, without populating
    // kObsoleteAutofillWalletImportEnabled. The latter defaults to true, but
    // that's not the case for the new replacement, i.e.
    // prefs::internal::kSyncPayments, so it needs to be populated manually to
    // migrate the old behavior.
    pref_service->SetBoolean(prefs::internal::kSyncPayments, true);
  }

  pref_service->ClearPref(kObsoleteAutofillWalletImportEnabled);
  pref_service->SetBoolean(kObsoleteAutofillWalletImportEnabledMigrated, true);
}

// static
void SyncPrefs::MigrateGlobalDataTypePrefsToAccount(
    PrefService* pref_service,
    const signin::GaiaIdHash& gaia_id_hash) {
  CHECK(gaia_id_hash.IsValid());

  // Note: This method does *not* ensure that the migration only runs once -
  // that's the caller's responsibility! (In practice, that's
  // MaybeMigrateSyncingUserToSignedIn()).

  ScopedDictPrefUpdate update_selected_types_dict(
      pref_service, prefs::internal::kSelectedTypesPerAccount);
  base::Value::Dict* account_settings =
      update_selected_types_dict->EnsureDict(gaia_id_hash.ToBase64());

  // The values of the "global" data type prefs get copied to the
  // account-specific ones.
  bool everything_enabled =
      pref_service->GetBoolean(prefs::internal::kSyncKeepEverythingSynced);
  // History and Tabs should remain enabled only if they were both enabled
  // previously, so they're specially tracked here.
  bool history_and_tabs_enabled = false;
  if (everything_enabled) {
    // Most of the per-account prefs default to "true", so nothing needs to be
    // done for those. The exceptions are History and Tabs, which need to be
    // enabled explicitly.
    history_and_tabs_enabled = true;
    // Additionally, on desktop, Passwords is considered disabled by default and
    // so also needs to be enabled explicitly.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    // TODO(b/314773312): Remove this when Uno is enabled.
    account_settings->Set(GetPrefNameForType(UserSelectableType::kPasswords),
                          true);
#endif
  } else {
    // "Sync everything" is off, so copy over the individual value for each
    // type.
    for (UserSelectableType type : UserSelectableTypeSet::All()) {
      const char* pref_name = GetPrefNameForType(type);
      CHECK(pref_name);
      // Copy value from global to per-account pref.
      account_settings->Set(pref_name, pref_service->GetBoolean(pref_name));
    }
    // Special case: History and Tabs remain enabled only if they were both
    // enabled previously.
    history_and_tabs_enabled =
        pref_service->GetBoolean(
            GetPrefNameForType(UserSelectableType::kHistory)) &&
        pref_service->GetBoolean(GetPrefNameForType(UserSelectableType::kTabs));
  }
  account_settings->Set(GetPrefNameForType(UserSelectableType::kHistory),
                        history_and_tabs_enabled);
  account_settings->Set(GetPrefNameForType(UserSelectableType::kTabs),
                        history_and_tabs_enabled);

  // Another special case: For custom passphrase users, "Addresses and more"
  // gets disabled by default. The reason is that for syncing custom passphrase
  // users, this toggle mapped to the legacy AUTOFILL_PROFILE type (which
  // supported custom passphrase), but for migrated users it maps to
  // CONTACT_INFO (which does not).
  std::optional<PassphraseType> passphrase_type = ProtoPassphraseInt32ToEnum(
      pref_service->GetInteger(prefs::internal::kSyncCachedPassphraseType));
  if (passphrase_type.has_value() && IsExplicitPassphrase(*passphrase_type)) {
    account_settings->Set(GetPrefNameForType(UserSelectableType::kAutofill),
                          false);
  }

  // Usually, the "SyncToSignin" migration (aka phase 2) will have completed
  // previously. But just in case it hasn't, make sure it doesn't run in the
  // future - it's not neeced, and in fact it might mess up some of the things
  // that were just migrated here.
  pref_service->SetInteger(prefs::internal::kSyncToSigninMigrationState,
                           kMigratedPart2AndFullyDone);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// static
void SyncPrefs::MaybeMigrateAutofillToPerAccountPref(
    PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(
          switches::kExplicitBrowserSigninUIOnDesktop)) {
    // Ensures the migration happens again if the experiment gets rolled back
    // then rolled out a second time.
    pref_service->ClearPref(kAutofillPerAccountPrefMigrationDone);
    return;
  }

  if (pref_service->GetBoolean(kAutofillPerAccountPrefMigrationDone)) {
    return;
  }
  pref_service->SetBoolean(kAutofillPerAccountPrefMigrationDone, true);

  std::string last_syncing_gaia_id =
      pref_service->GetString(::prefs::kGoogleServicesLastSyncingGaiaId);
  if (last_syncing_gaia_id.empty()) {
    return;
  }

  if (pref_service->GetBoolean(prefs::internal::kSyncKeepEverythingSynced)) {
    return;
  }

  for (auto user_selectable_type :
       {UserSelectableType::kPasswords, UserSelectableType::kAutofill}) {
    const char* const pref_name_for_type =
        GetPrefNameForType(user_selectable_type);
    if (pref_service->GetBoolean(pref_name_for_type)) {
      continue;
    }

    SetAccountKeyedPrefDictEntry(
        pref_service, prefs::internal::kSelectedTypesPerAccount,
        signin::GaiaIdHash::FromGaiaId(last_syncing_gaia_id),
        pref_name_for_type, base::Value(false));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void SyncPrefs::MarkPartialSyncToSigninMigrationFullyDone() {
  // If the first part of the migration has run, but the second part has not,
  // then mark the migration as fully done - at this point (after signout)
  // there's no more need for any migration.
  // In all other cases (migration never even started, or completed fully),
  // nothing to be done here.
  if (pref_service_->GetInteger(prefs::internal::kSyncToSigninMigrationState) ==
      kMigratedPart1ButNot2) {
    pref_service_->SetInteger(prefs::internal::kSyncToSigninMigrationState,
                              kMigratedPart2AndFullyDone);
  }
}

void SyncPrefs::SetPasswordSyncAllowed(bool allowed) {
  if (password_sync_allowed_ == allowed) {
    return;
  }

  password_sync_allowed_ = allowed;
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnSelectedTypesPrefChange();
  }
}

}  // namespace syncer
