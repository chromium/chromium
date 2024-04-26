// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_CONTROLLER_H_
#define COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_CONTROLLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/user_manager_export.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace user_manager {

class User;
class UserManager;

// MultiUserSignInPolicyController decides whether a user is allowed to be in a
// multi user sign-in session. It caches the multi user sign-in behavior pref
// backed by user policy into local state so that the value is available before
// the user login and checks if the meaning of the value is respected.
class USER_MANAGER_EXPORT MultiUserSignInPolicyController {
 public:
  MultiUserSignInPolicyController(PrefService* local_state,
                                  UserManager* user_manager);

  MultiUserSignInPolicyController(const MultiUserSignInPolicyController&) =
      delete;
  MultiUserSignInPolicyController& operator=(
      const MultiUserSignInPolicyController&) = delete;

  ~MultiUserSignInPolicyController();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the cached policy value for `user_email`.
  MultiUserSignInPolicy GetCachedValue(std::string_view user_email) const;

  // Returns true if user allowed to be in the current session.
  bool IsUserAllowedInSession(const std::string& user_email) const;

  // Starts to observe the multi-user signin policy for the given user.
  void StartObserving(User* user);

  // Stops to observe the multi-user signin policy for the given user.
  void StopObserving(User* user);

  // Removes the cached values for the given user.
  void RemoveCachedValues(std::string_view user_email);

 private:
  friend class MultiUserSignInPolicyControllerTest;

  // Sets the cached policy value.
  void SetCachedValue(std::string_view user_email,
                      MultiUserSignInPolicy policy);

  // Checks if all users are allowed in the current session.
  void CheckSessionUsers();

  // Invoked when user behavior pref value changes.
  void OnUserPrefChanged(User* user);

  raw_ptr<PrefService, DanglingUntriaged> local_state_;
  raw_ptr<UserManager> user_manager_;
  std::vector<std::unique_ptr<PrefChangeRegistrar>> pref_watchers_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_CONTROLLER_H_
