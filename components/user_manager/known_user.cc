// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/known_user.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_manager {
namespace {

// A vector pref of preferences of known users. All new preferences should be
// placed in this list.
const char kKnownUsers[] = "KnownUsers";

// Known user preferences keys (stored in Local State). All keys should be
// listed in kReservedKeys or kObsoleteKeys below.

// Key of canonical e-mail value.
const char kCanonicalEmail[] = "email";

// Key of obfuscated GAIA id value.
const char kGAIAIdKey[] = "gaia_id";

// Key of obfuscated object guid value for Active Directory accounts.
const char kObjGuidKey[] = "obj_guid";

// Key of account type.
const char kAccountTypeKey[] = "account_type";

// Key of whether this user ID refers to a SAML user.
const char kUsingSAMLKey[] = "using_saml";

// Key of whether this user authenticated via SAML using the principals API.
const char kIsUsingSAMLPrincipalsAPI[] = "using_saml_principals_api";

// Key of Device Id.
const char kDeviceId[] = "device_id";

// Key of GAPS cookie.
const char kGAPSCookie[] = "gaps_cookie";

// Key of the reason for re-auth.
const char kReauthReasonKey[] = "reauth_reason";

// Key for the GaiaId migration status.
const char kGaiaIdMigration[] = "gaia_id_migration";

// Key of the boolean flag telling if a minimal user home migration has been
// attempted. This flag is not used since M88 and is only kept here to be able
// to remove it from existing entries.
const char kMinimalMigrationAttemptedObsolete[] = "minimal_migration_attempted";

// Key of the boolean flag telling if user session requires policy.
const char kProfileRequiresPolicy[] = "profile_requires_policy";

// Key of the boolean flag telling if user is ephemeral and should be removed
// from the local state on logout.
const char kIsEphemeral[] = "is_ephemeral";

// Key of the list value that stores challenge-response authentication keys.
const char kChallengeResponseKeys[] = "challenge_response_keys";

const char kLastOnlineSignin[] = "last_online_singin";
const char kOfflineSigninLimitDeprecated[] = "offline_signin_limit";
const char kOfflineSigninLimit[] = "offline_signin_limit2";

// Key of the boolean flag telling if user is enterprise managed.
const char kIsEnterpriseManaged[] = "is_enterprise_managed";

// Key of the name of the entity (either a domain or email address) that manages
// the policies for this account.
const char kAccountManager[] = "enterprise_account_manager";

// Key of the last input method user used which is suitable for login/lock
// screen.
const char kLastInputMethod[] = "last_input_method";

// Key of the PIN auto submit length.
const char kPinAutosubmitLength[] = "pin_autosubmit_length";

// Key for the PIN auto submit backfill needed indicator.
const char kPinAutosubmitBackfillNeeded[] = "pin_autosubmit_backfill_needed";

// Sync token for SAML password multi-device sync
const char kPasswordSyncToken[] = "password_sync_token";

// Major version in which the user completed the onboarding flow.
const char kOnboardingCompletedVersion[] = "onboarding_completed_version";

// Last screen shown in the onboarding flow.
const char kPendingOnboardingScreen[] = "onboarding_screen_pending";

// List containing all the known user preferences keys.
const char* kReservedKeys[] = {kCanonicalEmail,
                               kGAIAIdKey,
                               kObjGuidKey,
                               kAccountTypeKey,
                               kUsingSAMLKey,
                               kIsUsingSAMLPrincipalsAPI,
                               kDeviceId,
                               kGAPSCookie,
                               kReauthReasonKey,
                               kGaiaIdMigration,
                               kProfileRequiresPolicy,
                               kIsEphemeral,
                               kChallengeResponseKeys,
                               kLastOnlineSignin,
                               kOfflineSigninLimit,
                               kIsEnterpriseManaged,
                               kAccountManager,
                               kLastInputMethod,
                               kPinAutosubmitLength,
                               kPinAutosubmitBackfillNeeded,
                               kPasswordSyncToken,
                               kOnboardingCompletedVersion,
                               kPendingOnboardingScreen};

// List containing all known user preference keys that used to be reserved and
// are now obsolete.
const char* kObsoleteKeys[] = {
    kMinimalMigrationAttemptedObsolete,
};

PrefService* GetLocalStateLegacy() {
  if (!UserManager::IsInitialized())
    return nullptr;

  return UserManager::Get()->GetLocalState();
}

// Checks if values in |dict| correspond with |account_id| identity.
bool UserMatches(const AccountId& account_id,
                 const base::DictionaryValue& dict) {
  std::string value;
  if (account_id.GetAccountType() != AccountType::UNKNOWN &&
      dict.GetString(kAccountTypeKey, &value) &&
      account_id.GetAccountType() != AccountId::StringToAccountType(value)) {
    return false;
  }

  // TODO(alemate): update code once user id is really a struct.
  // TODO(https://crbug.com/1190902): If the gaia id or GUID doesn't match,
  // this function should likely be returning false even if the e-mail matches.
  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE: {
      bool has_gaia_id = dict.GetString(kGAIAIdKey, &value);
      if (has_gaia_id && account_id.GetGaiaId() == value)
        return true;
      break;
    }
    case AccountType::ACTIVE_DIRECTORY: {
      bool has_obj_guid = dict.GetString(kObjGuidKey, &value);
      if (has_obj_guid && account_id.GetObjGuid() == value)
        return true;
      break;
    }
    case AccountType::UNKNOWN: {
    }
  }

  bool has_email = dict.GetString(kCanonicalEmail, &value);
  if (has_email && account_id.GetUserEmail() == value)
    return true;

  return false;
}

// Fills relevant |dict| values based on |account_id|.
void UpdateIdentity(const AccountId& account_id, base::DictionaryValue& dict) {
  if (!account_id.GetUserEmail().empty())
    dict.SetString(kCanonicalEmail, account_id.GetUserEmail());

  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE:
      if (!account_id.GetGaiaId().empty())
        dict.SetString(kGAIAIdKey, account_id.GetGaiaId());
      break;
    case AccountType::ACTIVE_DIRECTORY:
      if (!account_id.GetObjGuid().empty())
        dict.SetString(kObjGuidKey, account_id.GetObjGuid());
      break;
    case AccountType::UNKNOWN:
      return;
  }
  dict.SetString(kAccountTypeKey,
                 AccountId::AccountTypeToString(account_id.GetAccountType()));
}

}  // namespace

KnownUser::KnownUser(PrefService* local_state) : local_state_(local_state) {
  DCHECK(local_state);
}

KnownUser::~KnownUser() = default;

bool KnownUser::FindPrefs(const AccountId& account_id,
                          const base::DictionaryValue** out_value) {
  // UserManager is usually NULL in unit tests.
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY &&
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id)) {
    return false;
  }

  if (!account_id.is_valid())
    return false;

  const base::ListValue* known_users = local_state_->GetList(kKnownUsers);
  for (size_t i = 0; i < known_users->GetSize(); ++i) {
    const base::DictionaryValue* element = nullptr;
    if (known_users->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        known_users->GetDictionary(i, out_value);
        return true;
      }
    }
  }
  return false;
}

void KnownUser::UpdatePrefs(const AccountId& account_id,
                            const base::DictionaryValue& values,
                            bool clear) {
  // UserManager is usually NULL in unit tests.
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY &&
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id)) {
    return;
  }

  if (!account_id.is_valid())
    return;

  ListPrefUpdate update(local_state_, kKnownUsers);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* element = nullptr;
    if (update->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        if (clear)
          element->Clear();
        element->MergeDictionary(&values);
        UpdateIdentity(account_id, *element);
        return;
      }
    }
  }
  std::unique_ptr<base::DictionaryValue> new_value(new base::DictionaryValue());
  new_value->MergeDictionary(&values);
  UpdateIdentity(account_id, *new_value);
  update->Append(std::move(new_value));
}

bool KnownUser::GetStringPref(const AccountId& account_id,
                              const std::string& path,
                              std::string* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetString(path, out_value);
}

void KnownUser::SetStringPref(const AccountId& account_id,
                              const std::string& path,
                              const std::string& in_value) {
  base::DictionaryValue dict;
  dict.SetString(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool KnownUser::GetBooleanPref(const AccountId& account_id,
                               const std::string& path,
                               bool* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetBoolean(path, out_value);
}

void KnownUser::SetBooleanPref(const AccountId& account_id,
                               const std::string& path,
                               const bool in_value) {
  base::DictionaryValue dict;
  dict.SetBoolean(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool KnownUser::GetIntegerPref(const AccountId& account_id,
                               const std::string& path,
                               int* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;
  return user_pref_dict->GetInteger(path, out_value);
}

void KnownUser::SetIntegerPref(const AccountId& account_id,
                               const std::string& path,
                               const int in_value) {
  base::DictionaryValue dict;
  dict.SetInteger(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool KnownUser::GetPref(const AccountId& account_id,
                        const std::string& path,
                        const base::Value** out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  *out_value = user_pref_dict->FindPath(path);
  return *out_value != nullptr;
}

void KnownUser::SetPref(const AccountId& account_id,
                        const std::string& path,
                        base::Value in_value) {
  base::DictionaryValue dict;
  dict.SetPath(path, std::move(in_value));
  UpdatePrefs(account_id, dict, false);
}

void KnownUser::RemovePref(const AccountId& account_id,
                           const std::string& path) {
  // Prevent removing keys that are used internally.
  for (const std::string& key : kReservedKeys)
    CHECK_NE(path, key);

  ClearPref(account_id, path);
}

AccountId KnownUser::GetAccountId(const std::string& user_email,
                                  const std::string& id,
                                  const AccountType& account_type) {
  DCHECK((id.empty() && account_type == AccountType::UNKNOWN) ||
         (!id.empty() && account_type != AccountType::UNKNOWN));
  // In tests empty accounts are possible.
  if (user_email.empty() && id.empty() &&
      account_type == AccountType::UNKNOWN) {
    return EmptyAccountId();
  }

  AccountId result(EmptyAccountId());
  // UserManager is usually NULL in unit tests.
  if (account_type == AccountType::UNKNOWN && UserManager::IsInitialized() &&
      UserManager::Get()->GetPlatformKnownUserId(user_email, id, &result)) {
    return result;
  }

  std::string stored_gaia_id;
  std::string stored_obj_guid;
  const std::string sanitized_email =
      user_email.empty()
          ? std::string()
          : gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_email));

  if (!sanitized_email.empty()) {
    if (GetStringPref(AccountId::FromUserEmail(sanitized_email), kGAIAIdKey,
                      &stored_gaia_id)) {
      if (!id.empty()) {
        DCHECK(account_type == AccountType::GOOGLE);
        if (id != stored_gaia_id)
          LOG(ERROR) << "User gaia id has changed. Sync will not work.";
      }

      // gaia_id is associated with cryptohome.
      return AccountId::FromUserEmailGaiaId(sanitized_email, stored_gaia_id);
    }

    if (GetStringPref(AccountId::FromUserEmail(sanitized_email), kObjGuidKey,
                      &stored_obj_guid)) {
      if (!id.empty()) {
        DCHECK(account_type == AccountType::ACTIVE_DIRECTORY);
        if (id != stored_obj_guid)
          LOG(ERROR) << "User object guid has changed. Sync will not work.";
      }

      // obj_guid is associated with cryptohome.
      return AccountId::AdFromUserEmailObjGuid(sanitized_email,
                                               stored_obj_guid);
    }
  }

  std::string stored_email;
  switch (account_type) {
    case AccountType::GOOGLE:
      if (GetStringPref(AccountId::FromGaiaId(id), kCanonicalEmail,
                        &stored_email)) {
        return AccountId::FromUserEmailGaiaId(stored_email, id);
      }
      return AccountId::FromUserEmailGaiaId(sanitized_email, id);
    case AccountType::ACTIVE_DIRECTORY:
      if (GetStringPref(AccountId::AdFromObjGuid(id), kCanonicalEmail,
                        &stored_email)) {
        return AccountId::AdFromUserEmailObjGuid(stored_email, id);
      }
      return AccountId::AdFromUserEmailObjGuid(sanitized_email, id);
    case AccountType::UNKNOWN:
      return AccountId::FromUserEmail(sanitized_email);
  }
  NOTREACHED();
  return EmptyAccountId();
}

std::vector<AccountId> KnownUser::GetKnownAccountIds() {
  std::vector<AccountId> result;

  const base::ListValue* known_users = local_state_->GetList(kKnownUsers);
  for (size_t i = 0; i < known_users->GetSize(); ++i) {
    const base::DictionaryValue* element = nullptr;
    if (known_users->GetDictionary(i, &element)) {
      std::string email;
      std::string gaia_id;
      std::string obj_guid;
      const bool has_email = element->GetString(kCanonicalEmail, &email);
      const bool has_gaia_id = element->GetString(kGAIAIdKey, &gaia_id);
      const bool has_obj_guid = element->GetString(kObjGuidKey, &obj_guid);
      AccountType account_type = AccountType::GOOGLE;
      std::string account_type_string;
      if (element->GetString(kAccountTypeKey, &account_type_string)) {
        account_type = AccountId::StringToAccountType(account_type_string);
      }
      switch (account_type) {
        case AccountType::GOOGLE:
          if (has_email || has_gaia_id) {
            result.push_back(AccountId::FromUserEmailGaiaId(email, gaia_id));
          }
          break;
        case AccountType::ACTIVE_DIRECTORY:
          if (has_email && has_obj_guid) {
            result.push_back(
                AccountId::AdFromUserEmailObjGuid(email, obj_guid));
          }
          break;
        default:
          NOTREACHED() << "Unknown account type";
      }
    }
  }
  return result;
}

bool KnownUser::GetGaiaIdMigrationStatus(const AccountId& account_id,
                                         const std::string& subsystem) {
  bool migrated = false;

  if (GetBooleanPref(account_id,
                     std::string(kGaiaIdMigration) + "." + subsystem,
                     &migrated)) {
    return migrated;
  }

  return false;
}

void KnownUser::SetGaiaIdMigrationStatusDone(const AccountId& account_id,
                                             const std::string& subsystem) {
  SetBooleanPref(account_id, std::string(kGaiaIdMigration) + "." + subsystem,
                 true);
}

void KnownUser::SaveKnownUser(const AccountId& account_id) {
  const bool is_ephemeral =
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id);
  if (is_ephemeral &&
      account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY) {
    return;
  }
  UpdateId(account_id);
  local_state_->CommitPendingWrite();
}

void KnownUser::SetIsEphemeralUser(const AccountId& account_id,
                                   bool is_ephemeral) {
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY)
    return;
  SetBooleanPref(account_id, kIsEphemeral, is_ephemeral);
}

void KnownUser::UpdateId(const AccountId& account_id) {
  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE:
      SetStringPref(account_id, kGAIAIdKey, account_id.GetGaiaId());
      break;
    case AccountType::ACTIVE_DIRECTORY:
      SetStringPref(account_id, kObjGuidKey, account_id.GetObjGuid());
      break;
    case AccountType::UNKNOWN:
      return;
  }
  SetStringPref(account_id, kAccountTypeKey,
                AccountId::AccountTypeToString(account_id.GetAccountType()));
}

bool KnownUser::FindGaiaID(const AccountId& account_id,
                           std::string* out_value) {
  return GetStringPref(account_id, kGAIAIdKey, out_value);
}

void KnownUser::SetDeviceId(const AccountId& account_id,
                            const std::string& device_id) {
  const std::string known_device_id = GetDeviceId(account_id);
  if (!known_device_id.empty() && device_id != known_device_id) {
    NOTREACHED() << "Trying to change device ID for known user.";
  }
  SetStringPref(account_id, kDeviceId, device_id);
}

std::string KnownUser::GetDeviceId(const AccountId& account_id) {
  std::string device_id;
  if (GetStringPref(account_id, kDeviceId, &device_id)) {
    return device_id;
  }
  return std::string();
}

void KnownUser::SetGAPSCookie(const AccountId& account_id,
                              const std::string& gaps_cookie) {
  SetStringPref(account_id, kGAPSCookie, gaps_cookie);
}

std::string KnownUser::GetGAPSCookie(const AccountId& account_id) {
  std::string gaps_cookie;
  if (GetStringPref(account_id, kGAPSCookie, &gaps_cookie)) {
    return gaps_cookie;
  }
  return std::string();
}

void KnownUser::UpdateUsingSAML(const AccountId& account_id,
                                const bool using_saml) {
  SetBooleanPref(account_id, kUsingSAMLKey, using_saml);
}

bool KnownUser::IsUsingSAML(const AccountId& account_id) {
  bool using_saml;
  if (GetBooleanPref(account_id, kUsingSAMLKey, &using_saml))
    return using_saml;
  return false;
}

void KnownUser::UpdateIsUsingSAMLPrincipalsAPI(
    const AccountId& account_id,
    bool is_using_saml_principals_api) {
  SetBooleanPref(account_id, kIsUsingSAMLPrincipalsAPI,
                 is_using_saml_principals_api);
}

bool KnownUser::GetIsUsingSAMLPrincipalsAPI(const AccountId& account_id) {
  bool is_using_saml_principals_api;
  if (GetBooleanPref(account_id, kIsUsingSAMLPrincipalsAPI,
                     &is_using_saml_principals_api)) {
    return is_using_saml_principals_api;
  }
  return false;
}

void KnownUser::SetProfileRequiresPolicy(const AccountId& account_id,
                                         ProfileRequiresPolicy required) {
  DCHECK_NE(required, ProfileRequiresPolicy::kUnknown);
  SetBooleanPref(account_id, kProfileRequiresPolicy,
                 required == ProfileRequiresPolicy::kPolicyRequired);
}

ProfileRequiresPolicy KnownUser::GetProfileRequiresPolicy(
    const AccountId& account_id) {
  bool requires_policy;
  if (GetBooleanPref(account_id, kProfileRequiresPolicy, &requires_policy)) {
    return requires_policy ? ProfileRequiresPolicy::kPolicyRequired
                           : ProfileRequiresPolicy::kNoPolicyRequired;
  }
  return ProfileRequiresPolicy::kUnknown;
}

void KnownUser::ClearProfileRequiresPolicy(const AccountId& account_id) {
  ClearPref(account_id, kProfileRequiresPolicy);
}

void KnownUser::UpdateReauthReason(const AccountId& account_id,
                                   const int reauth_reason) {
  SetIntegerPref(account_id, kReauthReasonKey, reauth_reason);
}

bool KnownUser::FindReauthReason(const AccountId& account_id, int* out_value) {
  return GetIntegerPref(account_id, kReauthReasonKey, out_value);
}

void KnownUser::SetChallengeResponseKeys(const AccountId& account_id,
                                         base::Value value) {
  DCHECK(value.is_list());
  SetPref(account_id, kChallengeResponseKeys, std::move(value));
}

base::Value KnownUser::GetChallengeResponseKeys(const AccountId& account_id) {
  const base::Value* value = nullptr;
  if (!GetPref(account_id, kChallengeResponseKeys, &value) || !value->is_list())
    return base::Value();
  return value->Clone();
}

void KnownUser::SetLastOnlineSignin(const AccountId& account_id,
                                    base::Time time) {
  SetPref(account_id, kLastOnlineSignin, base::TimeToValue(time));
}

base::Time KnownUser::GetLastOnlineSignin(const AccountId& account_id) {
  const base::Value* value = nullptr;
  if (!GetPref(account_id, kLastOnlineSignin, &value))
    return base::Time();
  absl::optional<base::Time> time = base::ValueToTime(value);
  if (!time)
    return base::Time();
  return *time;
}

void KnownUser::SetOfflineSigninLimit(
    const AccountId& account_id,
    absl::optional<base::TimeDelta> time_delta) {
  if (!time_delta) {
    ClearPref(account_id, kOfflineSigninLimit);
  } else {
    SetPref(account_id, kOfflineSigninLimit,
            base::TimeDeltaToValue(time_delta.value()));
  }
}

absl::optional<base::TimeDelta> KnownUser::GetOfflineSigninLimit(
    const AccountId& account_id) {
  const base::Value* value = nullptr;
  if (!GetPref(account_id, kOfflineSigninLimit, &value))
    return absl::nullopt;
  absl::optional<base::TimeDelta> time_delta = base::ValueToTimeDelta(value);
  return time_delta;
}

void KnownUser::SetIsEnterpriseManaged(const AccountId& account_id,
                                       bool is_enterprise_managed) {
  SetBooleanPref(account_id, kIsEnterpriseManaged, is_enterprise_managed);
}

bool KnownUser::GetIsEnterpriseManaged(const AccountId& account_id) {
  bool is_enterprise_managed;
  if (GetBooleanPref(account_id, kIsEnterpriseManaged, &is_enterprise_managed))
    return is_enterprise_managed;
  return false;
}

void KnownUser::SetAccountManager(const AccountId& account_id,
                                  const std::string& manager) {
  SetStringPref(account_id, kAccountManager, manager);
}

bool KnownUser::GetAccountManager(const AccountId& account_id,
                                  std::string* manager) {
  return GetStringPref(account_id, kAccountManager, manager);
}

void KnownUser::SetUserLastLoginInputMethod(const AccountId& account_id,
                                            const std::string& input_method) {
  SetStringPref(account_id, kLastInputMethod, input_method);
}

bool KnownUser::GetUserLastInputMethod(const AccountId& account_id,
                                       std::string* input_method) {
  return GetStringPref(account_id, kLastInputMethod, input_method);
}

void KnownUser::SetUserPinLength(const AccountId& account_id, int pin_length) {
  SetIntegerPref(account_id, kPinAutosubmitLength, pin_length);
}

int KnownUser::GetUserPinLength(const AccountId& account_id) {
  int pin_length = 0;
  if (GetIntegerPref(account_id, kPinAutosubmitLength, &pin_length))
    return pin_length;
  return 0;
}

bool KnownUser::PinAutosubmitIsBackfillNeeded(const AccountId& account_id) {
  bool backfill_needed;
  if (GetBooleanPref(account_id, kPinAutosubmitBackfillNeeded,
                     &backfill_needed))
    return backfill_needed;
  // If the pref is not set, the pref needs to be backfilled.
  return true;
}

void KnownUser::PinAutosubmitSetBackfillNotNeeded(const AccountId& account_id) {
  SetBooleanPref(account_id, kPinAutosubmitBackfillNeeded, false);
}

void KnownUser::PinAutosubmitSetBackfillNeededForTests(
    const AccountId& account_id) {
  SetBooleanPref(account_id, kPinAutosubmitBackfillNeeded, true);
}

void KnownUser::SetPasswordSyncToken(const AccountId& account_id,
                                     const std::string& token) {
  SetStringPref(account_id, kPasswordSyncToken, token);
}

std::string KnownUser::GetPasswordSyncToken(const AccountId& account_id) {
  std::string token;
  if (GetStringPref(account_id, kPasswordSyncToken, &token))
    return token;
  // Return empty string if sync token was not set for the account yet.
  return std::string();
}

void KnownUser::SetOnboardingCompletedVersion(
    const AccountId& account_id,
    const absl::optional<base::Version> version) {
  if (!version) {
    ClearPref(account_id, kOnboardingCompletedVersion);
  } else {
    SetStringPref(account_id, kOnboardingCompletedVersion,
                  version.value().GetString());
  }
}

absl::optional<base::Version> KnownUser::GetOnboardingCompletedVersion(
    const AccountId& account_id) {
  std::string str_version;
  if (!GetStringPref(account_id, kOnboardingCompletedVersion, &str_version))
    return absl::nullopt;

  base::Version version = base::Version(str_version);
  if (!version.IsValid())
    return absl::nullopt;
  return version;
}

void KnownUser::RemoveOnboardingCompletedVersionForTests(
    const AccountId& account_id) {
  ClearPref(account_id, kOnboardingCompletedVersion);
}

void KnownUser::SetPendingOnboardingScreen(const AccountId& account_id,
                                           const std::string& screen) {
  SetStringPref(account_id, kPendingOnboardingScreen, screen);
}

void KnownUser::RemovePendingOnboardingScreen(const AccountId& account_id) {
  ClearPref(account_id, kPendingOnboardingScreen);
}

std::string KnownUser::GetPendingOnboardingScreen(const AccountId& account_id) {
  std::string screen;
  if (GetStringPref(account_id, kPendingOnboardingScreen, &screen))
    return screen;
  // Return empty string if no screen is pending.
  return std::string();
}

void KnownUser::ClearPref(const AccountId& account_id,
                          const std::string& path) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return;

  base::Value updated_user_pref = user_pref_dict->Clone();
  base::DictionaryValue* updated_user_pref_dict;
  updated_user_pref.GetAsDictionary(&updated_user_pref_dict);

  updated_user_pref_dict->RemovePath(path);
  UpdatePrefs(account_id, *updated_user_pref_dict, true);
}

void KnownUser::RemovePrefs(const AccountId& account_id) {
  if (!account_id.is_valid())
    return;

  ListPrefUpdate update(local_state_, kKnownUsers);
  base::Value::ListView update_view = update->GetList();
  for (auto it = update_view.begin(); it != update_view.end(); ++it) {
    base::DictionaryValue* element = nullptr;
    if (it->GetAsDictionary(&element)) {
      if (UserMatches(account_id, *element)) {
        update->EraseListIter(it);
        break;
      }
    }
  }
}

void KnownUser::CleanEphemeralUsers() {
  ListPrefUpdate update(local_state_, kKnownUsers);
  update->EraseListValueIf([](const auto& value) {
    if (!value.is_dict())
      return false;

    absl::optional<bool> is_ephemeral = value.FindBoolKey(kIsEphemeral);
    return is_ephemeral && *is_ephemeral;
  });
}

void KnownUser::CleanObsoletePrefs() {
  ListPrefUpdate update(local_state_, kKnownUsers);
  for (base::Value& user_entry : update.Get()->GetList()) {
    if (!user_entry.is_dict())
      continue;
    for (const std::string& key : kObsoleteKeys)
      user_entry.RemoveKey(key);

    // Migrate Offline signin limit to the new logic. Old logic stored 0 when
    // the limit was not set. New logic does not store anything when the limit
    // is not set because 0 is a legit value.
    const base::Value* value =
        user_entry.FindKey(kOfflineSigninLimitDeprecated);
    absl::optional<base::TimeDelta> new_value = base::ValueToTimeDelta(value);
    user_entry.RemoveKey(kOfflineSigninLimitDeprecated);
    if (new_value.has_value() && !new_value->is_zero()) {
      user_entry.SetKey(kOfflineSigninLimit,
                        base::TimeDeltaToValue(*new_value));
    }

    if (new_value.has_value() && new_value->is_zero()) {
      // The old logic wrongfully treated 0 as a legit value and forced the user
      // to sign-in online in the case that the value is set to 0. This would be
      // cached in the "UserForceOnlineSignin" pref. Reset it once during the
      // one-time migration to the new logic, if the value was 0.
      // TODO(https://crbug.com/1224318) Remove this around M95.
      const std::string* email = user_entry.FindStringKey(kCanonicalEmail);
      if (email) {
        // This is the same kUserForceOnlineSignin from user_manager_base.cc and
        // it's duplicated here as a temporary workaround.
        const char kUserForceOnlineSignin[] = "UserForceOnlineSignin";
        DictionaryPrefUpdate force_online_update(local_state_,
                                                 kUserForceOnlineSignin);
        force_online_update->SetKey(*email, base::Value(false));
      }
    }
  }
}

// static
void KnownUser::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kKnownUsers);
}

// --- Legacy interface ---
namespace known_user {

bool FindPrefs(const AccountId& account_id,
               const base::DictionaryValue** out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).FindPrefs(account_id, out_value);
}

void UpdatePrefs(const AccountId& account_id,
                 const base::DictionaryValue& values,
                 bool clear) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).UpdatePrefs(account_id, values, clear);
}

bool GetStringPref(const AccountId& account_id,
                   const std::string& path,
                   std::string* out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetStringPref(account_id, path, out_value);
}

void SetStringPref(const AccountId& account_id,
                   const std::string& path,
                   const std::string& in_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetStringPref(account_id, path, in_value);
}

bool GetBooleanPref(const AccountId& account_id,
                    const std::string& path,
                    bool* out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetBooleanPref(account_id, path, out_value);
}

void SetBooleanPref(const AccountId& account_id,
                    const std::string& path,
                    const bool in_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetBooleanPref(account_id, path, in_value);
}

bool GetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    int* out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetIntegerPref(account_id, path, out_value);
}

void SetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    const int in_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetIntegerPref(account_id, path, in_value);
}

bool GetPref(const AccountId& account_id,
             const std::string& path,
             const base::Value** out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetPref(account_id, path, out_value);
}

void SetPref(const AccountId& account_id,
             const std::string& path,
             base::Value in_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetPref(account_id, path, std::move(in_value));
}

void RemovePref(const AccountId& account_id, const std::string& path) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).RemovePref(account_id, path);
}

AccountId GetAccountId(const std::string& user_email,
                       const std::string& id,
                       const AccountType& account_type) {
  DCHECK((id.empty() && account_type == AccountType::UNKNOWN) ||
         (!id.empty() && account_type != AccountType::UNKNOWN));
  PrefService* local_state = GetLocalStateLegacy();
  if (local_state) {
    return KnownUser(local_state).GetAccountId(user_email, id, account_type);
  }

  // The handling of the local-state-not-initialized case is pretty complex - it
  // is KnownUser::GetAccountId with all queries assuming to return false.
  // This should be come unnecessary when all callers are migrated to the
  // KnownUser class interface (https://crbug.com/1150434) and thus responsible
  // to pass a valid |local_state| pointer.

  // In tests empty accounts are possible.
  if (user_email.empty() && id.empty() &&
      account_type == AccountType::UNKNOWN) {
    return EmptyAccountId();
  }
  AccountId result(EmptyAccountId());
  // UserManager is usually NULL in unit tests.
  if (account_type == AccountType::UNKNOWN && UserManager::IsInitialized() &&
      UserManager::Get()->GetPlatformKnownUserId(user_email, id, &result)) {
    return result;
  }
  const std::string sanitized_email =
      user_email.empty()
          ? std::string()
          : gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_email));
  std::string stored_email;
  switch (account_type) {
    case AccountType::GOOGLE:
      return AccountId::FromUserEmailGaiaId(sanitized_email, id);
    case AccountType::ACTIVE_DIRECTORY:
      return AccountId::AdFromUserEmailObjGuid(sanitized_email, id);
    case AccountType::UNKNOWN:
      return AccountId::FromUserEmail(sanitized_email);
  }
  NOTREACHED();
  return EmptyAccountId();
}

std::vector<AccountId> GetKnownAccountIds() {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return {};
  return KnownUser(local_state).GetKnownAccountIds();
}

bool GetGaiaIdMigrationStatus(const AccountId& account_id,
                              const std::string& subsystem) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetGaiaIdMigrationStatus(account_id, subsystem);
}

void SetGaiaIdMigrationStatusDone(const AccountId& account_id,
                                  const std::string& subsystem) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state)
      .SetGaiaIdMigrationStatusDone(account_id, subsystem);
}

void SaveKnownUser(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SaveKnownUser(account_id);
}

void UpdateId(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).UpdateId(account_id);
}

bool FindGaiaID(const AccountId& account_id, std::string* out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).FindGaiaID(account_id, out_value);
}

void SetDeviceId(const AccountId& account_id, const std::string& device_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetDeviceId(account_id, device_id);
}

std::string GetDeviceId(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return std::string();
  return KnownUser(local_state).GetDeviceId(account_id);
}

void SetGAPSCookie(const AccountId& account_id,
                   const std::string& gaps_cookie) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetGAPSCookie(account_id, gaps_cookie);
}

std::string GetGAPSCookie(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return std::string();
  return KnownUser(local_state).GetGAPSCookie(account_id);
}

void UpdateUsingSAML(const AccountId& account_id, const bool using_saml) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).UpdateUsingSAML(account_id, using_saml);
}

bool IsUsingSAML(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).IsUsingSAML(account_id);
}

void USER_MANAGER_EXPORT
UpdateIsUsingSAMLPrincipalsAPI(const AccountId& account_id,
                               bool is_using_saml_principals_api) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state)
      .UpdateIsUsingSAMLPrincipalsAPI(account_id, is_using_saml_principals_api);
}

bool USER_MANAGER_EXPORT
GetIsUsingSAMLPrincipalsAPI(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetIsUsingSAMLPrincipalsAPI(account_id);
}

void SetProfileRequiresPolicy(const AccountId& account_id,
                              ProfileRequiresPolicy required) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetProfileRequiresPolicy(account_id, required);
}

ProfileRequiresPolicy GetProfileRequiresPolicy(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return ProfileRequiresPolicy::kUnknown;
  return KnownUser(local_state).GetProfileRequiresPolicy(account_id);
}

void ClearProfileRequiresPolicy(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).ClearProfileRequiresPolicy(account_id);
}

void UpdateReauthReason(const AccountId& account_id, const int reauth_reason) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).UpdateReauthReason(account_id, reauth_reason);
}

bool FindReauthReason(const AccountId& account_id, int* out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).FindReauthReason(account_id, out_value);
}

void SetChallengeResponseKeys(const AccountId& account_id, base::Value value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state)
      .SetChallengeResponseKeys(account_id, std::move(value));
}

base::Value GetChallengeResponseKeys(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return base::Value();
  return KnownUser(local_state).GetChallengeResponseKeys(account_id);
}

void SetLastOnlineSignin(const AccountId& account_id, base::Time time) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetLastOnlineSignin(account_id, time);
}

base::Time GetLastOnlineSignin(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return base::Time();
  return KnownUser(local_state).GetLastOnlineSignin(account_id);
}

void SetOfflineSigninLimit(const AccountId& account_id,
                           absl::optional<base::TimeDelta> time_delta) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetOfflineSigninLimit(account_id, time_delta);
}

absl::optional<base::TimeDelta> GetOfflineSigninLimit(
    const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return absl::nullopt;
  return KnownUser(local_state).GetOfflineSigninLimit(account_id);
}

void SetIsEnterpriseManaged(const AccountId& account_id,
                            bool is_enterprise_managed) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state)
      .SetIsEnterpriseManaged(account_id, is_enterprise_managed);
}

bool GetIsEnterpriseManaged(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetIsEnterpriseManaged(account_id);
}

void SetAccountManager(const AccountId& account_id,
                       const std::string& manager) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetAccountManager(account_id, manager);
}

bool GetAccountManager(const AccountId& account_id, std::string* manager) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetAccountManager(account_id, manager);
}

void SetUserLastLoginInputMethod(const AccountId& account_id,
                                 const std::string& input_method) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state)
      .SetUserLastLoginInputMethod(account_id, input_method);
}

bool GetUserLastInputMethod(const AccountId& account_id,
                            std::string* input_method) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state)
      .GetUserLastInputMethod(account_id, input_method);
}

void SetUserPinLength(const AccountId& account_id, int pin_length) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetUserPinLength(account_id, pin_length);
}

int GetUserPinLength(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return 0;
  return KnownUser(local_state).GetUserPinLength(account_id);
}

bool PinAutosubmitIsBackfillNeeded(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state) {
    // If the pref is not set, the pref needs to be backfilled.
    return true;
  }
  return KnownUser(local_state).PinAutosubmitIsBackfillNeeded(account_id);
}

void PinAutosubmitSetBackfillNotNeeded(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).PinAutosubmitSetBackfillNotNeeded(account_id);
}

void PinAutosubmitSetBackfillNeededForTests(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state)
      .PinAutosubmitSetBackfillNeededForTests(account_id);
}

void SetPasswordSyncToken(const AccountId& account_id,
                          const std::string& token) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetPasswordSyncToken(account_id, token);
}

std::string GetPasswordSyncToken(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return std::string();
  return KnownUser(local_state).GetPasswordSyncToken(account_id);
}

}  // namespace known_user
}  // namespace user_manager
