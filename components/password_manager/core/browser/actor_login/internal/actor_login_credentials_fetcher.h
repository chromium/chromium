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
  enum class Status {
    kSuccess,

    // Password statuses:
    kFillingNotAllowed,
  };

  virtual ~ActorLoginCredentialsFetcher() = default;

  using FetchResultCallback =
      base::OnceCallback<void(std::vector<Credential>, Status)>;

  // Fetches credentials asynchronously.
  virtual void Fetch(FetchResultCallback callback) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIALS_FETCHER_H_
