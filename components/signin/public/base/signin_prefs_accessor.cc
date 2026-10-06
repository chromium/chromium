// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs_accessor.h"

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

// Name of the main pref dictionary holding the account dictionaries of the
// underlying prefs, with the key as the GaiaIds. The prefs stored through
// this dict here are information not directly related to the account itself,
// only metadata.
constexpr char kSigninAccountPrefs[] = "signin.accounts_metadata_dict";

std::string FormatPrefPathForCheck(base::span<const std::string_view> parents,
                                   std::string_view key) {
  if (parents.empty()) {
    return std::string(key);
  }
  return base::StrCat({base::JoinString(parents, "/"), "/", key});
}

void CheckRegisteredPrefType(SigninPrefsRegistry::PassKey pass_key,
                             base::span<const std::string_view> parents,
                             std::string_view key,
                             base::Value::Type expected_type,
                             bool expected_is_timestamp = false) {
  const auto* pref_def = SigninPrefsRegistry::Find(pass_key, parents, key);
  CHECK(pref_def) << "Unregistered SigninPrefs key: "
                  << FormatPrefPathForCheck(parents, key);
  CHECK_EQ(pref_def->type, expected_type)
      << "Type mismatch for SigninPrefs key: "
      << FormatPrefPathForCheck(parents, key);
  CHECK_EQ(pref_def->is_timestamp, expected_is_timestamp)
      << "Timestamp mismatch for SigninPrefs key: "
      << FormatPrefPathForCheck(parents, key);
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

}  // namespace

SigninPrefsAccessor::SigninPrefsAccessor(PrefService& pref_service, PassKey)
    : pref_service_(pref_service) {}

SigninPrefsAccessor::~SigninPrefsAccessor() = default;

// static
void SigninPrefsAccessor::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                               PassKey) {
  registry->RegisterDictionaryPref(kSigninAccountPrefs);
}

// static
void SigninPrefsAccessor::ObserveChanges(PrefChangeRegistrar& registrar,
                                         base::RepeatingClosure callback,
                                         PassKey) {
  registrar.Add(kSigninAccountPrefs, std::move(callback));
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

const base::DictValue& SigninPrefsAccessor::GetAccountPrefsDict() const {
  return pref_service_->GetDict(kSigninAccountPrefs);
}

bool SigninPrefsAccessor::HasAccountPrefs(const GaiaId& gaia_id) const {
  return pref_service_->GetDict(kSigninAccountPrefs)
      .contains(gaia_id.ToString());
}

size_t SigninPrefsAccessor::RemoveAllAccountPrefsExcept(
    const base::flat_set<GaiaId>& gaia_ids_to_keep) {
  std::vector<std::string> accounts_prefs_to_remove;
  for (const auto [account_key, _] :
       pref_service_->GetDict(kSigninAccountPrefs)) {
    if (!gaia_ids_to_keep.contains(GaiaId(account_key))) {
      accounts_prefs_to_remove.push_back(account_key);
    }
  }
  if (accounts_prefs_to_remove.empty()) {
    return 0;
  }

  ScopedDictPrefUpdate scoped_update(&pref_service_.get(), kSigninAccountPrefs);
  for (std::string_view account_prefs_to_remove : accounts_prefs_to_remove) {
    scoped_update->Remove(account_prefs_to_remove);
  }

  return accounts_prefs_to_remove.size();
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
  int new_value = base::ClampAdd(dict->FindInt(key).value_or(0), 1);
  dict->Set(key, new_value);
  return new_value;
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
  CHECK(SigninPrefsRegistry::Find(base::PassKey<SigninPrefsAccessor>(), parents,
                                  key))
      << "Unregistered SigninPrefs key: "
      << FormatPrefPathForCheck(parents, key);
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
