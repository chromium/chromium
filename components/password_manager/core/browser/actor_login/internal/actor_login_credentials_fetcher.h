// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIALS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIALS_FETCHER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace actor_login {

// Interface for fetching credentials for the Actor Login feature.
class ActorLoginCredentialsFetcher {
 public:
  // Interface for status reported by a fetcher.
  class Status {
   public:
    virtual ~Status() = default;

    // Returns an error if this status represents a condition that should
    // cause the overall `GetCredentials` request to fail.
    // Currently, this is a workaround for the fact that the password fetch can
    // fail due to filling not being allowed.
    // TODO(crbug.com/478799141): Remove this once we stop returning filling not
    // allowed as an overall error. Potentially we can remove the Status
    // altogether if we log everything through MQLS per-fetcher.
    virtual std::optional<ActorLoginError> GetGlobalError() const = 0;
  };

  virtual ~ActorLoginCredentialsFetcher() = default;

  using FetchResultCallback = base::OnceCallback<void(std::vector<Credential>,
                                                      std::unique_ptr<Status>)>;

  // Fetches credentials asynchronously.
  virtual void Fetch(FetchResultCallback callback) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIALS_FETCHER_H_
