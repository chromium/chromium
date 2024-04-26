// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"

namespace user_manager {

MultiUserSignInPolicyController::MultiUserSignInPolicyController(
    PrefService* local_state,
    UserManager* user_manager)
    : local_state_(local_state), user_manager_(user_manager) {}

MultiUserSignInPolicyController::~MultiUserSignInPolicyController() = default;

// static
void MultiUserSignInPolicyController::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCachedMultiProfileUserBehavior);
}

bool MultiUserSignInPolicyController::IsUserAllowedInSession(
    const std::string& user_email) const {
  const User* primary_user = user_manager_->GetPrimaryUser();
  std::string primary_user_email;
  if (primary_user) {
    primary_user_email = primary_user->GetAccountId().GetUserEmail();
  }

  // Always allow if there is no primary user or user being checked is the
  // primary user.
  if (primary_user_email.empty() || primary_user_email == user_email) {
    return true;
  }

  auto primary_user_policy = GetMultiUserSignInPolicy(primary_user);
  if (primary_user_policy == MultiUserSignInPolicy::kNotAllowed) {
    return false;
  }

  // The user must have 'unrestricted' policy to be a secondary user.
  const auto policy = GetCachedValue(user_email);
  return policy == MultiUserSignInPolicy::kUnrestricted;
}

void MultiUserSignInPolicyController::StartObserving(User* user) {
  // Profile name could be empty during tests.
  if (user->GetAccountId().GetUserEmail().empty() || !user->GetProfilePrefs()) {
    return;
  }

  auto registrar = std::make_unique<PrefChangeRegistrar>();
  registrar->Init(user->GetProfilePrefs());
  registrar->Add(
      prefs::kMultiProfileUserBehaviorPref,
      base::BindRepeating(&MultiUserSignInPolicyController::OnUserPrefChanged,
                          base::Unretained(this), user));
  pref_watchers_.push_back(std::move(registrar));

  OnUserPrefChanged(user);
}

void MultiUserSignInPolicyController::StopObserving(User* user) {
  auto* prefs = user->GetProfilePrefs();
  std::erase_if(pref_watchers_, [prefs](auto& registrar) {
    return registrar->prefs() == prefs;
  });
}

void MultiUserSignInPolicyController::RemoveCachedValues(
    std::string_view user_email) {
  ScopedDictPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->Remove(user_email);
}

MultiUserSignInPolicy MultiUserSignInPolicyController::GetCachedValue(
    std::string_view user_email) const {
  const base::Value::Dict& dict =
      local_state_->GetDict(prefs::kCachedMultiProfileUserBehavior);

  const std::string* value = dict.FindString(user_email);
  if (!value) {
    return MultiUserSignInPolicy::kUnrestricted;
  }

  return ParseMultiUserSignInPolicyPref(*value).value_or(
      MultiUserSignInPolicy::kUnrestricted);
}

void MultiUserSignInPolicyController::SetCachedValue(
    std::string_view user_email,
    MultiUserSignInPolicy policy) {
  ScopedDictPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->Set(user_email, MultiUserSignInPolicyToPrefValue(policy));
}

void MultiUserSignInPolicyController::CheckSessionUsers() {
  for (const User* user : user_manager_->GetLoggedInUsers()) {
    const std::string& user_email = user->GetAccountId().GetUserEmail();
    if (!IsUserAllowedInSession(user_email)) {
      user_manager_->NotifyUserNotAllowed(user_email);
      return;
    }
  }
}

void MultiUserSignInPolicyController::OnUserPrefChanged(User* user) {
  std::string user_email = user->GetAccountId().GetUserEmail();
  CHECK(!user_email.empty());

  PrefService* prefs = user->GetProfilePrefs();
  if (prefs->FindPreference(prefs::kMultiProfileUserBehaviorPref)
          ->IsDefaultValue()) {
    // Migration code to clear cached default behavior.
    // TODO(xiyuan): Remove this after M35.
    ScopedDictPrefUpdate update(local_state_,
                                prefs::kCachedMultiProfileUserBehavior);
    update->Remove(user_email);
  } else {
    auto policy = ParseMultiUserSignInPolicyPref(
        prefs->GetString(prefs::kMultiProfileUserBehaviorPref));
    SetCachedValue(user_email,
                   policy.value_or(MultiUserSignInPolicy::kUnrestricted));
  }

  CheckSessionUsers();
}

}  // namespace user_manager
