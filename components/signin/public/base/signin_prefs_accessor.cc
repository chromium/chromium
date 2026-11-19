// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs_accessor.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/json/values_util.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_prefs_keys.h"
#include "components/signin/public/base/signin_prefs_registry.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

// Name of the pref dictionary holding single-account metadata dictionaries,
// keyed by individual `GaiaId`s. The prefs stored through this dict are
// information not directly related to the account itself, only metadata.
// Multi-account metadata is stored separately in
// `kMultipleAccountsMetadataList`.
constexpr char kSigninAccountPrefs[] = "signin.accounts_metadata_dict";

// Name of the list pref holding metadata keyed by an unordered set of at
// least two distinct `GaiaId`s (stored sorted by `GaiaId` to avoid duplicates).
// Each entry is a dictionary:
// {
//   "gaia_ids": ["<gaia_id_1>", "<gaia_id_2>", ...],
//   "prefs": { "<pref_key>": <value>, ... }
// }
constexpr char kMultipleAccountsMetadataList[] =
    "signin.multiple_accounts_metadata_list";

constexpr char kMultiAccountGaiaIdsKey[] = "gaia_ids";
constexpr char kMultiAccountPrefsKey[] = "prefs";

std::string FormatPrefPathForCheck(base::span<const std::string_view> parents,
                                   std::string_view key) {
  if (parents.empty()) {
    return std::string(key);
  }
  return base::StrCat({base::JoinString(parents, "/"), "/", key});
}

const SigninPrefsRegistry::PrefDescriptor* RequireRegisteredPref(
    SigninPrefsRegistry::PassKey pass_key,
    base::span<const std::string_view> parents,
    std::string_view key,
    bool expected_is_multiple_accounts) {
  const auto* pref_def =
      expected_is_multiple_accounts
          ? SigninPrefsRegistry::FindMultipleAccounts(pass_key, parents, key)
          : SigninPrefsRegistry::Find(pass_key, parents, key);
  const bool exists_in_other_scope =
      !pref_def && (expected_is_multiple_accounts
                        ? SigninPrefsRegistry::Find(pass_key, parents, key)
                        : SigninPrefsRegistry::FindMultipleAccounts(
                              pass_key, parents, key)) != nullptr;
  CHECK(pref_def) << (exists_in_other_scope
                          ? "Scope mismatch for SigninPrefs key: "
                          : "Unregistered SigninPrefs key: ")
                  << FormatPrefPathForCheck(parents, key);
  return pref_def;
}

void CheckRegisteredPrefType(SigninPrefsRegistry::PassKey pass_key,
                             base::span<const std::string_view> parents,
                             std::string_view key,
                             base::Value::Type expected_type,
                             bool expected_is_timestamp = false,
                             bool expected_is_multiple_accounts = false) {
  const auto* pref_def = RequireRegisteredPref(pass_key, parents, key,
                                               expected_is_multiple_accounts);
  CHECK_EQ(pref_def->type, expected_type)
      << "Type mismatch for SigninPrefs key: "
      << FormatPrefPathForCheck(parents, key);
  CHECK_EQ(pref_def->is_timestamp, expected_is_timestamp)
      << "Timestamp mismatch for SigninPrefs key: "
      << FormatPrefPathForCheck(parents, key);
}

int IncrementIntInDict(base::DictValue& dict, std::string_view key) {
  int new_value = base::ClampAdd(dict.FindInt(key).value_or(0), 1);
  dict.Set(key, new_value);
  return new_value;
}

base::DictValue* EnsureNestedDict(base::DictValue& account_dict,
                                  base::span<const std::string_view> parents) {
  base::DictValue* dict = &account_dict;
  for (std::string_view parent : parents) {
    dict = dict->EnsureDict(parent);
  }
  return dict;
}

const base::DictValue* FindNestedDict(
    const base::DictValue* account_dict,
    base::span<const std::string_view> parents) {
  const base::DictValue* dict = account_dict;
  for (std::string_view parent : parents) {
    if (!dict) {
      return nullptr;
    }
    dict = dict->FindDict(parent);
  }
  return dict;
}

void ValidateMultipleAccountsKey(const base::flat_set<GaiaId>& gaia_ids) {
  CHECK_GE(gaia_ids.size(), 2u);
  for (const GaiaId& gaia_id : gaia_ids) {
    CHECK(!gaia_id.empty());
  }
}

bool IsValidMultipleAccountsGaiaIdsList(const base::ListValue* gaia_ids_list) {
  if (!gaia_ids_list || gaia_ids_list->size() < 2) {
    return false;
  }
  const std::string* previous = nullptr;
  for (const base::Value& entry : *gaia_ids_list) {
    const std::string* current = entry.GetIfString();
    if (!current || current->empty() || (previous && *previous >= *current)) {
      return false;
    }
    previous = current;
  }
  return true;
}

bool MatchesMultipleAccountsEntry(const base::DictValue& entry_dict,
                                  const base::flat_set<GaiaId>& gaia_ids) {
  const base::ListValue* entry_gaia_ids =
      entry_dict.FindList(kMultiAccountGaiaIdsKey);
  if (!entry_gaia_ids || entry_gaia_ids->size() != gaia_ids.size()) {
    return false;
  }
  auto it = gaia_ids.begin();
  for (const base::Value& entry_value : *entry_gaia_ids) {
    const std::string* entry_gaia_id = entry_value.GetIfString();
    if (!entry_gaia_id || *entry_gaia_id != it->ToString()) {
      return false;
    }
    ++it;
  }
  return true;
}

const base::DictValue* FindMultipleAccountsPrefsDict(
    const PrefService& pref_service,
    const base::flat_set<GaiaId>& gaia_ids) {
  for (const base::Value& entry_value :
       pref_service.GetList(kMultipleAccountsMetadataList)) {
    const base::DictValue* entry_dict = entry_value.GetIfDict();
    if (entry_dict && MatchesMultipleAccountsEntry(*entry_dict, gaia_ids)) {
      return entry_dict->FindDict(kMultiAccountPrefsKey);
    }
  }
  return nullptr;
}

base::DictValue* EnsureMultipleAccountsPrefsDict(
    base::ListValue& entries,
    const base::flat_set<GaiaId>& gaia_ids) {
  for (base::Value& entry_value : entries) {
    base::DictValue* entry_dict = entry_value.GetIfDict();
    if (entry_dict && MatchesMultipleAccountsEntry(*entry_dict, gaia_ids)) {
      return entry_dict->EnsureDict(kMultiAccountPrefsKey);
    }
  }
  base::DictValue new_entry;
  base::ListValue gaia_id_list;
  for (const GaiaId& gaia_id : gaia_ids) {
    gaia_id_list.Append(gaia_id.ToString());
  }
  new_entry.Set(kMultiAccountGaiaIdsKey, std::move(gaia_id_list));
  new_entry.Set(kMultiAccountPrefsKey, base::DictValue());
  entries.Append(std::move(new_entry));
  return entries.back().GetDict().FindDict(kMultiAccountPrefsKey);
}

bool IsValidMultipleAccountsEntry(const base::Value& entry_value) {
  const base::DictValue* entry_dict = entry_value.GetIfDict();
  return entry_dict &&
         IsValidMultipleAccountsGaiaIdsList(
             entry_dict->FindList(kMultiAccountGaiaIdsKey)) &&
         entry_dict->FindDict(kMultiAccountPrefsKey);
}

bool ShouldKeepMultipleAccountsEntry(
    const base::Value& entry_value,
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  if (!IsValidMultipleAccountsEntry(entry_value)) {
    return false;
  }
  const base::ListValue* entry_gaia_ids =
      entry_value.GetDict().FindList(kMultiAccountGaiaIdsKey);
  for (const base::Value& entry : *entry_gaia_ids) {
    if (!gaia_ids_to_keep.contains(GaiaId(entry.GetString()))) {
      return false;
    }
  }
  return true;
}

bool HasAnyPrunableMultipleAccountsEntry(
    const base::ListValue& entries,
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  for (const base::Value& entry_value : entries) {
    if (!ShouldKeepMultipleAccountsEntry(entry_value, gaia_ids_to_keep)) {
      return true;
    }
  }
  return false;
}

bool HasDeprecatedSigninPref(const base::DictValue& prefs_dict) {
  for (std::string_view deprecated_pref :
       signin::internal::kDeprecatedSigninPrefs) {
    if (prefs_dict.contains(deprecated_pref)) {
      return true;
    }
  }
  return false;
}

bool NeedsMultipleAccountsMigration(const base::ListValue& entries) {
  for (const base::Value& entry_value : entries) {
    if (!IsValidMultipleAccountsEntry(entry_value)) {
      return true;
    }
    const base::DictValue* prefs_dict =
        entry_value.GetDict().FindDict(kMultiAccountPrefsKey);
    if (prefs_dict->empty() || HasDeprecatedSigninPref(*prefs_dict)) {
      return true;
    }
  }
  return false;
}

size_t PruneSingleAccountPrefs(PrefService& pref_service,
                               const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  std::vector<std::string> accounts_prefs_to_remove;
  for (const auto [account_key, _] :
       pref_service.GetDict(kSigninAccountPrefs)) {
    if (!gaia_ids_to_keep.contains(GaiaId(account_key))) {
      accounts_prefs_to_remove.push_back(account_key);
    }
  }
  if (accounts_prefs_to_remove.empty()) {
    return 0;
  }
  ScopedDictPrefUpdate scoped_update(&pref_service, kSigninAccountPrefs);
  for (std::string_view account_prefs_to_remove : accounts_prefs_to_remove) {
    scoped_update->Remove(account_prefs_to_remove);
  }
  return accounts_prefs_to_remove.size();
}

void PruneMultipleAccountsPrefs(
    PrefService& pref_service,
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  if (!HasAnyPrunableMultipleAccountsEntry(
          pref_service.GetList(kMultipleAccountsMetadataList),
          gaia_ids_to_keep)) {
    return;
  }
  ScopedListPrefUpdate scoped_update(&pref_service,
                                     kMultipleAccountsMetadataList);
  scoped_update->EraseIf([&gaia_ids_to_keep](const base::Value& entry_value) {
    return !ShouldKeepMultipleAccountsEntry(entry_value, gaia_ids_to_keep);
  });
}

}  // namespace

SigninPrefsAccessor::SigninPrefsAccessor(PrefService& pref_service, PassKey)
    : pref_service_(pref_service) {}

SigninPrefsAccessor::~SigninPrefsAccessor() = default;

// static
void SigninPrefsAccessor::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                               PassKey) {
  registry->RegisterDictionaryPref(kSigninAccountPrefs);
  registry->RegisterListPref(kMultipleAccountsMetadataList);
}

// static
void SigninPrefsAccessor::ObserveChanges(PrefChangeRegistrar& registrar,
                                         base::RepeatingClosure callback,
                                         PassKey) {
  registrar.Add(kSigninAccountPrefs, callback);
  registrar.Add(kMultipleAccountsMetadataList, std::move(callback));
}

void SigninPrefsAccessor::MigrateObsoleteAccountPrefs() {
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  // Corrupted (non-dict) account entries are collected and erased after the
  // iteration, as erasing while iterating would invalidate the iterator.
  std::vector<std::string> corrupted_account_keys;
  for (auto [account_key, account_value] : scoped_update.Get()) {
    base::DictValue* account_dict = account_value.GetIfDict();
    if (!account_dict) {
      corrupted_account_keys.push_back(account_key);
      continue;
    }
    for (std::string_view deprecated_pref :
         signin::internal::kDeprecatedSigninPrefs) {
      account_dict->Remove(deprecated_pref);
    }
  }
  for (std::string_view corrupted_account_key : corrupted_account_keys) {
    scoped_update->Remove(corrupted_account_key);
  }
}

void SigninPrefsAccessor::MigrateObsoleteMultipleAccountsPrefs() {
  if (!pref_service_->FindPreference(kMultipleAccountsMetadataList)
           ->IsUserControlled() ||
      !NeedsMultipleAccountsMigration(
          pref_service_->GetList(kMultipleAccountsMetadataList))) {
    return;
  }
  ScopedListPrefUpdate multi_update(&pref_service_.get(),
                                    kMultipleAccountsMetadataList);
  for (base::Value& entry_value : *multi_update) {
    if (!IsValidMultipleAccountsEntry(entry_value)) {
      continue;
    }
    base::DictValue* prefs_dict =
        entry_value.GetDict().FindDict(kMultiAccountPrefsKey);
    for (std::string_view deprecated_pref :
         signin::internal::kDeprecatedSigninPrefs) {
      prefs_dict->Remove(deprecated_pref);
    }
  }
  // Remove corrupted entries and entries left with no active prefs after
  // iteration, avoiding manual iterator erasure during the loop above.
  multi_update->EraseIf([](const base::Value& entry_value) {
    return !IsValidMultipleAccountsEntry(entry_value) ||
           entry_value.GetDict().FindDict(kMultiAccountPrefsKey)->empty();
  });
}

const base::DictValue& SigninPrefsAccessor::GetAccountPrefsDict() const {
  return pref_service_->GetDict(kSigninAccountPrefs);
}

const base::ListValue& SigninPrefsAccessor::GetMultipleAccountsPrefsList()
    const {
  return pref_service_->GetList(kMultipleAccountsMetadataList);
}

bool SigninPrefsAccessor::HasAccountPrefs(const GaiaId& gaia_id) const {
  return pref_service_->GetDict(kSigninAccountPrefs)
      .contains(gaia_id.ToString());
}

size_t SigninPrefsAccessor::RemoveAllAccountPrefsExcept(
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  const size_t removed_single_accounts_count =
      PruneSingleAccountPrefs(*pref_service_, gaia_ids_to_keep);
  PruneMultipleAccountsPrefs(*pref_service_, gaia_ids_to_keep);
  return removed_single_accounts_count;
}

int SigninPrefsAccessor::IncrementIntPref(
    const GaiaId& gaia_id,
    std::string_view key,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), parents, key,
                          base::Value::Type::INTEGER);
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict =
      EnsureNestedDict(*scoped_update->EnsureDict(gaia_id.ToString()), parents);
  return IncrementIntInDict(*dict, key);
}

int SigninPrefsAccessor::GetIntPref(
    const GaiaId& gaia_id,
    std::string_view key,
    base::span<const std::string_view> parents) const {
  return MaybeGetIntPref(gaia_id, key, parents).value_or(0);
}

std::optional<int> SigninPrefsAccessor::MaybeGetIntPref(
    const GaiaId& gaia_id,
    std::string_view key,
    base::span<const std::string_view> parents) const {
  CHECK(!gaia_id.empty());
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), parents, key,
                          base::Value::Type::INTEGER);
  const base::DictValue* dict = FindNestedDict(
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString()),
      parents);
  return dict ? dict->FindInt(key) : std::nullopt;
}

void SigninPrefsAccessor::SetIntPref(
    const GaiaId& gaia_id,
    std::string_view key,
    int value,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), parents, key,
                          base::Value::Type::INTEGER);
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict =
      EnsureNestedDict(*scoped_update->EnsureDict(gaia_id.ToString()), parents);
  dict->Set(key, value);
}

void SigninPrefsAccessor::SetBooleanPref(
    const GaiaId& gaia_id,
    std::string_view key,
    bool value,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), parents, key,
                          base::Value::Type::BOOLEAN);
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict =
      EnsureNestedDict(*scoped_update->EnsureDict(gaia_id.ToString()), parents);
  dict->Set(key, value);
}

bool SigninPrefsAccessor::GetBooleanPref(
    const GaiaId& gaia_id,
    std::string_view key,
    base::span<const std::string_view> parents) const {
  CHECK(!gaia_id.empty());
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), parents, key,
                          base::Value::Type::BOOLEAN);
  const base::DictValue* dict = FindNestedDict(
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString()),
      parents);
  return dict ? dict->FindBool(key).value_or(false) : false;
}

void SigninPrefsAccessor::SetTimePref(
    const GaiaId& gaia_id,
    std::string_view key,
    base::Time value,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), parents, key,
                          base::Value::Type::STRING,
                          /*expected_is_timestamp=*/true);
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict =
      EnsureNestedDict(*scoped_update->EnsureDict(gaia_id.ToString()), parents);
  dict->Set(key, base::TimeToValue(value));
}

std::optional<base::Time> SigninPrefsAccessor::GetTimePref(
    const GaiaId& gaia_id,
    std::string_view key,
    base::span<const std::string_view> parents) const {
  CHECK(!gaia_id.empty());
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), parents, key,
                          base::Value::Type::STRING,
                          /*expected_is_timestamp=*/true);
  const base::DictValue* dict = FindNestedDict(
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString()),
      parents);
  if (!dict) {
    return std::nullopt;
  }
  const base::Value* value = dict->Find(key);
  return value ? base::ValueToTime(value) : std::nullopt;
}

void SigninPrefsAccessor::SetValue(const GaiaId& gaia_id,
                                   std::string_view key,
                                   base::Value value,
                                   base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  RequireRegisteredPref(base::PassKey<SigninPrefsAccessor>(), parents, key,
                        /*expected_is_multiple_accounts=*/false);
  CHECK(SigninPrefsRegistry::IsValidValue(base::PassKey<SigninPrefsAccessor>(),
                                          parents, key, value))
      << "Invalid value for SigninPrefs key: "
      << FormatPrefPathForCheck(parents, key);
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict =
      EnsureNestedDict(*scoped_update->EnsureDict(gaia_id.ToString()), parents);
  dict->Set(key, std::move(value));
}

bool SigninPrefsAccessor::ClearPref(
    const GaiaId& gaia_id,
    std::string_view key,
    base::span<const std::string_view> parents) {
  CHECK(!gaia_id.empty());
  RequireRegisteredPref(base::PassKey<SigninPrefsAccessor>(), parents, key,
                        /*expected_is_multiple_accounts=*/false);
  // Check whether the target key exists before opening `ScopedDictPrefUpdate`
  // so that clearing an absent key or account does not dirty the pref store or
  // fire `PrefChangeRegistrar` notifications. Only a value controlled by the
  // user pref store is considered, because that is the store
  // `ScopedDictPrefUpdate` mutates; values coming from other stores cannot be
  // removed through it.
  // `PrefService::GetUserPrefValue()` is deliberately not used: it asserts
  // when the persisted value is not a dict, which corrupted on-disk data can
  // trigger. `GetDict()` type-checks every store and falls back to the default
  // (empty) dict instead, so a corrupted value is treated as having nothing to
  // clear.
  if (!pref_service_->FindPreference(kSigninAccountPrefs)->IsUserControlled()) {
    return false;
  }
  const base::DictValue* const_dict = FindNestedDict(
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString()),
      parents);
  if (!const_dict || !const_dict->contains(key)) {
    return false;
  }

  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* dict = scoped_update->FindDict(gaia_id.ToString());
  for (std::string_view parent : parents) {
    CHECK(dict);
    dict = dict->FindDict(parent);
  }
  CHECK(dict);
  return dict->Remove(key);
}

size_t SigninPrefsAccessor::ClearPrefForAllAccounts(
    std::string_view key,
    base::span<const std::string_view> parents) {
  RequireRegisteredPref(base::PassKey<SigninPrefsAccessor>(), parents, key,
                        /*expected_is_multiple_accounts=*/false);
  if (!pref_service_->FindPreference(kSigninAccountPrefs)->IsUserControlled()) {
    return 0;
  }

  std::vector<std::string> accounts_to_clear;
  for (const auto [account_key, account_value] :
       pref_service_->GetDict(kSigninAccountPrefs)) {
    const base::DictValue* const_dict =
        FindNestedDict(account_value.GetIfDict(), parents);
    if (const_dict && const_dict->contains(key)) {
      accounts_to_clear.push_back(account_key);
    }
  }
  if (accounts_to_clear.empty()) {
    return 0;
  }

  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  for (std::string_view account_key : accounts_to_clear) {
    base::DictValue* dict = scoped_update->FindDict(account_key);
    for (std::string_view parent : parents) {
      CHECK(dict);
      dict = dict->FindDict(parent);
    }
    CHECK(dict);
    CHECK(dict->Remove(key));
  }
  return accounts_to_clear.size();
}

int SigninPrefsAccessor::IncrementMultipleAccountsIntPref(
    const base::flat_set<GaiaId>& gaia_ids,
    std::string_view key) {
  ValidateMultipleAccountsKey(gaia_ids);
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), {}, key,
                          base::Value::Type::INTEGER,
                          /*expected_is_timestamp=*/false,
                          /*expected_is_multiple_accounts=*/true);
  ScopedListPrefUpdate scoped_update(&pref_service_.get(),
                                     kMultipleAccountsMetadataList);
  base::DictValue* dict =
      EnsureMultipleAccountsPrefsDict(*scoped_update, gaia_ids);
  return IncrementIntInDict(*dict, key);
}

int SigninPrefsAccessor::GetMultipleAccountsIntPref(
    const base::flat_set<GaiaId>& gaia_ids,
    std::string_view key) const {
  ValidateMultipleAccountsKey(gaia_ids);
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), {}, key,
                          base::Value::Type::INTEGER,
                          /*expected_is_timestamp=*/false,
                          /*expected_is_multiple_accounts=*/true);
  const base::DictValue* dict =
      FindMultipleAccountsPrefsDict(*pref_service_, gaia_ids);
  return dict ? dict->FindInt(key).value_or(0) : 0;
}

void SigninPrefsAccessor::SetMultipleAccountsBooleanPref(
    const base::flat_set<GaiaId>& gaia_ids,
    std::string_view key,
    bool value) {
  ValidateMultipleAccountsKey(gaia_ids);
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), {}, key,
                          base::Value::Type::BOOLEAN,
                          /*expected_is_timestamp=*/false,
                          /*expected_is_multiple_accounts=*/true);
  ScopedListPrefUpdate scoped_update(&pref_service_.get(),
                                     kMultipleAccountsMetadataList);
  base::DictValue* dict =
      EnsureMultipleAccountsPrefsDict(*scoped_update, gaia_ids);
  dict->Set(key, value);
}

bool SigninPrefsAccessor::GetMultipleAccountsBooleanPref(
    const base::flat_set<GaiaId>& gaia_ids,
    std::string_view key) const {
  ValidateMultipleAccountsKey(gaia_ids);
  CheckRegisteredPrefType(base::PassKey<SigninPrefsAccessor>(), {}, key,
                          base::Value::Type::BOOLEAN,
                          /*expected_is_timestamp=*/false,
                          /*expected_is_multiple_accounts=*/true);
  const base::DictValue* dict =
      FindMultipleAccountsPrefsDict(*pref_service_, gaia_ids);
  return dict ? dict->FindBool(key).value_or(false) : false;
}

void SigninPrefsAccessor::SetDeprecatedPrefForTesting(const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  base::DictValue* account_dict = scoped_update->EnsureDict(gaia_id.ToString());
  account_dict->Set(signin::internal::kDeprecatedTestingPref, 123);
}

std::optional<int> SigninPrefsAccessor::GetDeprecatedPrefForTesting(
    const GaiaId& gaia_id) const {
  CHECK(!gaia_id.empty());
  const base::DictValue* account_dict =
      pref_service_->GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  if (!account_dict) {
    return std::nullopt;
  }
  return account_dict->FindInt(signin::internal::kDeprecatedTestingPref);
}
