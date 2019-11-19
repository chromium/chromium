// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/known_user.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace user_manager {
namespace known_user {
namespace {

// A vector pref of preferences of known users. All new preferences should be
// placed in this list.
const char kKnownUsers[] = "KnownUsers";

// Known user preferences keys (stored in Local State). All keys should be
// listed in kReservedKeys below.

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
// attempted.
const char kMinimalMigrationAttempted[] = "minimal_migration_attempted";

// Key of the boolean flag telling if user session requires policy.
const char kProfileRequiresPolicy[] = "profile_requires_policy";

// Key of the boolean flag telling if user is ephemeral and should be removed
// from the local state on logout.
const char kIsEphemeral[] = "is_ephemeral";

// Key of the list value that stores challenge-response authentication keys.
const char kChallengeResponseKeys[] = "challenge_response_keys";

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
                               kMinimalMigrationAttempted,
                               kProfileRequiresPolicy,
                               kIsEphemeral,
                               kChallengeResponseKeys};

PrefService* GetLocalState() {
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

void ClearPref(const AccountId& account_id, const std::string& path) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return;

  base::Value updated_user_pref = user_pref_dict->Clone();
  base::DictionaryValue* updated_user_pref_dict;
  updated_user_pref.GetAsDictionary(&updated_user_pref_dict);

  updated_user_pref_dict->RemovePath(path);
  UpdatePrefs(account_id, *updated_user_pref_dict, true);
}

}  // namespace

bool FindPrefs(const AccountId& account_id,
               const base::DictionaryValue** out_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return false;

  // UserManager is usually NULL in unit tests.
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY &&
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id)) {
    return false;
  }

  const base::ListValue* known_users = local_state->GetList(kKnownUsers);
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

void UpdatePrefs(const AccountId& account_id,
                 const base::DictionaryValue& values,
                 bool clear) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  // UserManager is usually NULL in unit tests.
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY &&
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id)) {
    return;
  }

  ListPrefUpdate update(local_state, kKnownUsers);
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

bool GetStringPref(const AccountId& account_id,
                   const std::string& path,
                   std::string* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetString(path, out_value);
}

void SetStringPref(const AccountId& account_id,
                   const std::string& path,
                   const std::string& in_value) {
  base::DictionaryValue dict;
  dict.SetString(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool GetBooleanPref(const AccountId& account_id,
                    const std::string& path,
                    bool* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetBoolean(path, out_value);
}

void SetBooleanPref(const AccountId& account_id,
                    const std::string& path,
                    const bool in_value) {
  base::DictionaryValue dict;
  dict.SetBoolean(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool GetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    int* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;
  return user_pref_dict->GetInteger(path, out_value);
}

void SetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    const int in_value) {
  base::DictionaryValue dict;
  dict.SetInteger(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool GetPref(const AccountId& account_id,
             const std::string& path,
             const base::Value** out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  *out_value = user_pref_dict->FindPath(path);
  return *out_value != nullptr;
}

void SetPref(const AccountId& account_id,
             const std::string& path,
             base::Value in_value) {
  base::DictionaryValue dict;
  dict.SetPath(path, std::move(in_value));
  UpdatePrefs(account_id, dict, false);
}

void RemovePref(const AccountId& account_id, const std::string& path) {
  // Prevent removing keys that are used internally.
  for (const std::string& key : kReservedKeys)
    CHECK_NE(path, key);

  ClearPref(account_id, path);
}

AccountId GetAccountId(const std::string& user_email,
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

std::vector<AccountId> GetKnownAccountIds() {
  std::vector<AccountId> result;
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return result;

  const base::ListValue* known_users = local_state->GetList(kKnownUsers);
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

bool GetGaiaIdMigrationStatus(const AccountId& account_id,
                              const std::string& subsystem) {
  bool migrated = false;

  if (GetBooleanPref(account_id,
                     std::string(kGaiaIdMigration) + "." + subsystem,
                     &migrated)) {
    return migrated;
  }

  return false;
}

void SetGaiaIdMigrationStatusDone(const AccountId& account_id,
                                  const std::string& subsystem) {
  SetBooleanPref(account_id, std::string(kGaiaIdMigration) + "." + subsystem,
                 true);
}

void SaveKnownUser(const AccountId& account_id) {
  const bool is_ephemeral =
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id);
  if (is_ephemeral &&
      account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY) {
    return;
  }
  UpdateId(account_id);
  GetLocalState()->CommitPendingWrite();
}

void SetIsEphemeralUser(const AccountId& account_id, bool is_ephemeral) {
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY)
    return;
  SetBooleanPref(account_id, kIsEphemeral, is_ephemeral);
}

void UpdateGaiaID(const AccountId& account_id, const std::string& gaia_id) {
  SetStringPref(account_id, kGAIAIdKey, gaia_id);
  SetStringPref(account_id, kAccountTypeKey,
                AccountId::AccountTypeToString(AccountType::GOOGLE));
}

void UpdateId(const AccountId& account_id) {
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

bool FindGaiaID(const AccountId& account_id, std::string* out_value) {
  return GetStringPref(account_id, kGAIAIdKey, out_value);
}

void SetDeviceId(const AccountId& account_id, const std::string& device_id) {
  const std::string known_device_id = GetDeviceId(account_id);
  if (!known_device_id.empty() && device_id != known_device_id) {
    NOTREACHED() << "Trying to change device ID for known user.";
  }
  SetStringPref(account_id, kDeviceId, device_id);
}

std::string GetDeviceId(const AccountId& account_id) {
  std::string device_id;
  if (GetStringPref(account_id, kDeviceId, &device_id)) {
    return device_id;
  }
  return std::string();
}

void SetGAPSCookie(const AccountId& account_id,
                   const std::string& gaps_cookie) {
  SetStringPref(account_id, kGAPSCookie, gaps_cookie);
}

std::string GetGAPSCookie(const AccountId& account_id) {
  std::string gaps_cookie;
  if (GetStringPref(account_id, kGAPSCookie, &gaps_cookie)) {
    return gaps_cookie;
  }
  return std::string();
}

void UpdateUsingSAML(const AccountId& account_id, const bool using_saml) {
  SetBooleanPref(account_id, kUsingSAMLKey, using_saml);
}

bool IsUsingSAML(const AccountId& account_id) {
  bool using_saml;
  if (GetBooleanPref(account_id, kUsingSAMLKey, &using_saml))
    return using_saml;
  return false;
}

void USER_MANAGER_EXPORT
UpdateIsUsingSAMLPrincipalsAPI(const AccountId& account_id,
                               bool is_using_saml_principals_api) {
  SetBooleanPref(account_id, kIsUsingSAMLPrincipalsAPI,
                 is_using_saml_principals_api);
}

bool USER_MANAGER_EXPORT
GetIsUsingSAMLPrincipalsAPI(const AccountId& account_id) {
  bool is_using_saml_principals_api;
  if (GetBooleanPref(account_id, kIsUsingSAMLPrincipalsAPI,
                     &is_using_saml_principals_api)) {
    return is_using_saml_principals_api;
  }
  return false;
}

void SetProfileRequiresPolicy(const AccountId& account_id,
                              ProfileRequiresPolicy required) {
  DCHECK_NE(required, ProfileRequiresPolicy::kUnknown);
  SetBooleanPref(account_id, kProfileRequiresPolicy,
                 required == ProfileRequiresPolicy::kPolicyRequired);
}

ProfileRequiresPolicy GetProfileRequiresPolicy(const AccountId& account_id) {
  bool requires_policy;
  if (GetBooleanPref(account_id, kProfileRequiresPolicy, &requires_policy)) {
    return requires_policy ? ProfileRequiresPolicy::kPolicyRequired
                           : ProfileRequiresPolicy::kNoPolicyRequired;
  }
  return ProfileRequiresPolicy::kUnknown;
}

void ClearProfileRequiresPolicy(const AccountId& account_id) {
  ClearPref(account_id, kProfileRequiresPolicy);
}

void UpdateReauthReason(const AccountId& account_id, const int reauth_reason) {
  SetIntegerPref(account_id, kReauthReasonKey, reauth_reason);
}

bool FindReauthReason(const AccountId& account_id, int* out_value) {
  return GetIntegerPref(account_id, kReauthReasonKey, out_value);
}

bool WasUserHomeMinimalMigrationAttempted(const AccountId& account_id) {
  bool minimal_migration_attempted;
  const bool pref_set = GetBooleanPref(account_id, kMinimalMigrationAttempted,
                                       &minimal_migration_attempted);
  if (pref_set)
    return minimal_migration_attempted;

  // If we haven't recorded that a minimal migration has been attempted, assume
  // no.
  return false;
}

void SetUserHomeMinimalMigrationAttempted(const AccountId& account_id,
                                          bool minimal_migration_attempted) {
  SetBooleanPref(account_id, kMinimalMigrationAttempted,
                 minimal_migration_attempted);
}

void SetChallengeResponseKeys(const AccountId& account_id, base::Value value) {
  DCHECK(value.is_list());
  SetPref(account_id, kChallengeResponseKeys, std::move(value));
}

base::Value GetChallengeResponseKeys(const AccountId& account_id) {
  const base::Value* value = nullptr;
  if (!GetPref(account_id, kChallengeResponseKeys, &value) || !value->is_list())
    return base::Value();
  return value->Clone();
}

void RemovePrefs(const AccountId& account_id) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* element = nullptr;
    if (update->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        update->Remove(i, nullptr);
        break;
      }
    }
  }
}

void CleanEphemeralUsers() {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  auto& list_storage = update->GetList();
  for (auto it = list_storage.begin(); it < list_storage.end();) {
    bool remove = false;
    base::DictionaryValue* element = nullptr;
    if (update->GetDictionary(std::distance(list_storage.begin(), it),
                              &element)) {
      base::Value* is_ephemeral = element->FindKey(kIsEphemeral);
      if (is_ephemeral && is_ephemeral->GetBool())
        remove = true;
    }
    if (remove)
      it = list_storage.erase(it);
    else
      it++;
  }
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kKnownUsers);
}

}  // namespace known_user
}  // namespace user_manager
