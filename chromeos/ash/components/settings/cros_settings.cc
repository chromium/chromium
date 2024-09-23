// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/cros_settings.h"

#include <stddef.h>

#include <string_view>

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/system_settings_provider.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

namespace {
CrosSettings* g_cros_settings = nullptr;
}  // namespace

// static
bool CrosSettings::IsInitialized() {
  return g_cros_settings;
}

// static
CrosSettings* CrosSettings::Get() {
  CHECK(g_cros_settings);
  return g_cros_settings;
}

// static
void CrosSettings::SetInstance(CrosSettings* cros_settings) {
  CHECK(!g_cros_settings || !cros_settings);
  g_cros_settings = cros_settings;
}

CrosSettings::CrosSettings() = default;

CrosSettings::~CrosSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool CrosSettings::IsCrosSettings(std::string_view path) {
  return base::StartsWith(path, kCrosSettingsPrefix,
                          base::CompareCase::SENSITIVE);
}

const base::Value* CrosSettings::GetPref(std::string_view path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CrosSettingsProvider* provider = GetProvider(path);
  if (provider)
    return provider->Get(path);
  NOTREACHED_IN_MIGRATION()
      << path << " preference was not found in the signed settings.";
  return nullptr;
}

CrosSettingsProvider::TrustedStatus CrosSettings::PrepareTrustedValues(
    base::OnceClosure callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < providers_.size(); ++i) {
    CrosSettingsProvider::TrustedStatus status =
        providers_[i]->PrepareTrustedValues(&callback);
    if (status != CrosSettingsProvider::TRUSTED)
      return status;
  }
  return CrosSettingsProvider::TRUSTED;
}

bool CrosSettings::GetBoolean(std::string_view path, bool* bool_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_bool()) {
    *bool_value = value->GetBool();
    return true;
  }
  return false;
}

bool CrosSettings::GetInteger(std::string_view path, int* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_int()) {
    *out_value = value->GetInt();
    return true;
  }
  return false;
}

bool CrosSettings::GetDouble(std::string_view path, double* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `GetIfDouble` incapsulates type check.
  std::optional<double> maybe_value = GetPref(path)->GetIfDouble();
  if (maybe_value.has_value()) {
    *out_value = maybe_value.value();
    return true;
  }
  return false;
}

bool CrosSettings::GetString(std::string_view path,
                             std::string* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_string()) {
    *out_value = value->GetString();
    return true;
  }
  return false;
}

bool CrosSettings::GetList(std::string_view path,
                           const base::Value::List** out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_list()) {
    *out_value = &value->GetList();
    return true;
  }
  return false;
}

bool CrosSettings::GetDictionary(std::string_view path,
                                 const base::Value::Dict** out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value = GetPref(path);
  if (value && value->is_dict()) {
    *out_value = &value->GetDict();
    return true;
  }
  return false;
}

bool CrosSettings::IsUserAllowlisted(
    const std::string& username,
    bool* wildcard_match,
    const std::optional<user_manager::UserType>& user_type) const {
  // Skip allowlist check for tests.
  if (switches::ShouldSkipOobePostLogin()) {
    return true;
  }

  bool allow_new_user = false;
  GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return true;

  if (FindEmailInList(kAccountsPrefUsers, username, wildcard_match))
    return true;

  bool family_link_allowed = false;
  GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed, &family_link_allowed);
  return family_link_allowed && user_type == user_manager::UserType::kChild;
}

bool CrosSettings::FindEmailInList(const std::string& path,
                                   const std::string& email,
                                   bool* wildcard_match) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::List* list;
  if (!GetList(path, &list)) {
    if (wildcard_match)
      *wildcard_match = false;
    return false;
  }

  return FindEmailInList(*list, email, wildcard_match);
}

// static
bool CrosSettings::FindEmailInList(const base::Value::List& list,
                                   const std::string& email,
                                   bool* wildcard_match) {
  std::string canonicalized_email(
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(email)));
  std::string wildcard_email;
  std::string::size_type at_pos = canonicalized_email.find('@');
  if (at_pos != std::string::npos) {
    wildcard_email =
        std::string("*").append(canonicalized_email.substr(at_pos));
  }

  if (wildcard_match)
    *wildcard_match = false;

  bool found_wildcard_match = false;
  for (const auto& entry : list) {
    if (!entry.is_string()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    std::string canonicalized_entry(
        gaia::CanonicalizeEmail(gaia::SanitizeEmail(entry.GetString())));

    if (canonicalized_entry != wildcard_email &&
        canonicalized_entry == canonicalized_email) {
      return true;
    }

    // If there is a wildcard match, don't exit early. There might be an exact
    // match further down the list that should take precedence if present.
    if (canonicalized_entry == wildcard_email)
      found_wildcard_match = true;
  }

  if (wildcard_match)
    *wildcard_match = found_wildcard_match;

  return found_wildcard_match;
}

void CrosSettings::SetSupervisedUserCrosSettingsProvider(
    std::unique_ptr<CrosSettingsProvider> provider) {
  CHECK(!supervised_user_cros_settings_provider_);
  supervised_user_cros_settings_provider_ = provider.get();
  AddSettingsProvider(std::move(provider));
}

bool CrosSettings::AddSettingsProvider(
    std::unique_ptr<CrosSettingsProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CrosSettingsProvider* provider_ptr = provider.get();
  providers_.push_back(std::move(provider));

  // Allow the provider to notify this object when settings have changed.
  // Providers instantiated inside this class will have the same callback
  // passed to their constructor, but doing it here allows for providers
  // to be instantiated outside this class.
  CrosSettingsProvider::NotifyObserversCallback notify_cb(base::BindRepeating(
      &CrosSettings::FireObservers, base::Unretained(this)));
  provider_ptr->SetNotifyObserversCallback(notify_cb);
  return true;
}

std::unique_ptr<CrosSettingsProvider> CrosSettings::RemoveSettingsProvider(
    CrosSettingsProvider* provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = base::ranges::find(providers_, provider,
                               &std::unique_ptr<CrosSettingsProvider>::get);
  if (it != providers_.end()) {
    std::unique_ptr<CrosSettingsProvider> ptr = std::move(*it);
    providers_.erase(it);
    return ptr;
  }
  return nullptr;
}

base::CallbackListSubscription CrosSettings::AddSettingsObserver(
    const std::string& path,
    base::RepeatingClosure callback) {
  DCHECK(!path.empty());
  DCHECK(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetProvider(path));

  // Get the callback registry associated with the path.
  base::RepeatingClosureList* registry = nullptr;
  auto observer_iterator = settings_observers_.find(path);
  if (observer_iterator == settings_observers_.end()) {
    settings_observers_[path] = std::make_unique<base::RepeatingClosureList>();
    registry = settings_observers_[path].get();
  } else {
    registry = observer_iterator->second.get();
  }

  return registry->Add(std::move(callback));
}

CrosSettingsProvider* CrosSettings::GetProvider(std::string_view path) const {
  for (const auto& provider : providers_) {
    if (provider->HandlesSetting(path)) {
      return provider.get();
    }
  }
  return nullptr;
}

void CrosSettings::FireObservers(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto observer_iterator = settings_observers_.find(path);
  if (observer_iterator == settings_observers_.end())
    return;

  observer_iterator->second->Notify();
}

}  // namespace ash
