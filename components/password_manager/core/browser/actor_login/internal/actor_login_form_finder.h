// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/form_data.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordFormManager;
class PasswordManagerClient;
}  // namespace password_manager

namespace url {
class Origin;
}  // namespace url

// Helper class to find all the login forms.
class ActorLoginFormFinder {
 public:
  explicit ActorLoginFormFinder(
      password_manager::PasswordManagerClient* client);
  ~ActorLoginFormFinder();

  ActorLoginFormFinder(const ActorLoginFormFinder&) = delete;
  ActorLoginFormFinder& operator=(const ActorLoginFormFinder&) = delete;

  // Extracts the site or app origin (scheme, host, port) from a URL as a
  // string.
  static std::u16string GetSourceSiteOrAppFromUrl(const GURL& url);

  // Finds the most suitable PasswordFormManager for a sign-in form associated
  // with a given origin from the form cache. It prioritizes forms in the
  // primary main frame.
  password_manager::PasswordFormManager* GetSigninFormManager(
      const url::Origin& origin);

  // Returns all the `PasswordFormManager`s that are allowed for `origin` and
  // with a valid parsed login form
  std::vector<password_manager::PasswordFormManager*>
  GetEligibleLoginFormManagers(const url::Origin& origin);

 private:
  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_
