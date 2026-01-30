// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credentials_fetcher.h"

namespace actor_login {

// Helper class to get credentials for the Actor Login feature.
// It starts multiple fetchers in parallel and merges the results once all are
// done. It then responds with the result asynchronously.
class ActorLoginGetCredentialsHelper {
 public:
  ActorLoginGetCredentialsHelper(
      std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers,
      CredentialsOrErrorReply callback);

  ActorLoginGetCredentialsHelper(const ActorLoginGetCredentialsHelper&) =
      delete;
  ActorLoginGetCredentialsHelper& operator=(
      const ActorLoginGetCredentialsHelper&) = delete;

  ~ActorLoginGetCredentialsHelper();

 private:
  struct FetchResult {
    FetchResult();
    FetchResult(std::vector<Credential> credentials,
                std::unique_ptr<ActorLoginCredentialsFetcher::Status> status);
    FetchResult(const FetchResult&) = delete;
    FetchResult& operator=(const FetchResult&) = delete;
    FetchResult(FetchResult&&);
    FetchResult& operator=(FetchResult&&);
    ~FetchResult();

    std::vector<Credential> credentials;
    std::unique_ptr<ActorLoginCredentialsFetcher::Status> status;
  };

  void OnAllFetchesCompleted(std::vector<FetchResult> results);

  std::vector<Credential> MergeCredentials(std::vector<FetchResult> results);

  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers_;
  CredentialsOrErrorReply callback_;

  base::WeakPtrFactory<ActorLoginGetCredentialsHelper> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_
