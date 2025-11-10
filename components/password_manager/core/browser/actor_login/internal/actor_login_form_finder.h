// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class GURL;

namespace password_manager {
class PasswordFormManager;
class PasswordManagerClient;
class PasswordManagerDriver;
}  // namespace password_manager

namespace url {
class Origin;
}  // namespace url

namespace autofill {
class FormRendererId;
}

namespace actor_login {
// Helper class to find all the login forms.
class ActorLoginFormFinder {
 public:
  // A callback that receives all eligible login form managers found on the
  // page.
  using EligibleManagersCallback = base::OnceCallback<void(
      std::vector<password_manager::PasswordFormManager*>)>;
  using DriverFormKey =
      std::pair<autofill::FormRendererId,
                base::WeakPtr<password_manager::PasswordManagerDriver>>;

  explicit ActorLoginFormFinder(
      password_manager::PasswordManagerClient* client);
  ~ActorLoginFormFinder();

  ActorLoginFormFinder(const ActorLoginFormFinder&) = delete;
  ActorLoginFormFinder& operator=(const ActorLoginFormFinder&) = delete;

  // Extracts the site or app origin (scheme, host, port) from a URL as a
  // string.
  static std::u16string GetSourceSiteOrAppFromUrl(const GURL& url);

  // Finds the most suitable `PasswordFormManager` from the list.
  // It prioritizes forms in the primary main frame.
  static password_manager::PasswordFormManager* GetSigninFormManager(
      const std::vector<password_manager::PasswordFormManager*>&
          eligible_managers);

  // Returns all the `PasswordFormManager`s that are allowed for `origin` and
  // with a valid parsed login form
  std::vector<password_manager::PasswordFormManager*>
  GetEligibleLoginFormManagers(const url::Origin& origin);

  // Asynchronously finds all `PasswordFormManager`s that are associated with
  // `origin` and have a valid parsed login form. Invokes `callback` with the
  // results.
  void GetEligibleLoginFormManagersAsync(const url::Origin& origin,
                                         EligibleManagersCallback callback);

 private:
  // Callback for when all candidate forms have been checked for eligibility.
  // It filters the candidates and passes the final eligible list to `callback`.
  void OnAllEligibleChecksCompleted(
      EligibleManagersCallback callback,
      std::vector<std::pair<
          std::pair<autofill::FormRendererId,
                    base::WeakPtr<password_manager::PasswordManagerDriver>>,
          bool>> results);

  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  base::WeakPtrFactory<ActorLoginFormFinder> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_
