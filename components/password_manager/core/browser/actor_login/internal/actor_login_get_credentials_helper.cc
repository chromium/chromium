// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include <vector>

#include "base/barrier_callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace actor_login {

ActorLoginGetCredentialsHelper::ActorLoginGetCredentialsHelper(
    std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers,
    CredentialsOrErrorReply callback)
    : fetchers_(std::move(fetchers)), callback_(std::move(callback)) {
  if (fetchers_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::vector<Credential>()));
    return;
  }

  auto barrier_callback = base::BarrierCallback<FetchResult>(
      fetchers_.size(),
      base::BindOnce(&ActorLoginGetCredentialsHelper::OnAllFetchesCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  for (std::unique_ptr<ActorLoginCredentialsFetcher>& fetcher : fetchers_) {
    fetcher->Fetch(base::BindOnce(
        [](base::RepeatingCallback<void(FetchResult)> barrier,
           std::vector<Credential> credentials,
           std::unique_ptr<ActorLoginCredentialsFetcher::Status> status) {
          barrier.Run({std::move(credentials), std::move(status)});
        },
        barrier_callback));
  }
}

ActorLoginGetCredentialsHelper::~ActorLoginGetCredentialsHelper() = default;

ActorLoginGetCredentialsHelper::FetchResult::FetchResult() = default;

ActorLoginGetCredentialsHelper::FetchResult::FetchResult(
    std::vector<Credential> credentials,
    std::unique_ptr<ActorLoginCredentialsFetcher::Status> status)
    : credentials(std::move(credentials)), status(std::move(status)) {}

ActorLoginGetCredentialsHelper::FetchResult::FetchResult(FetchResult&&) =
    default;

ActorLoginGetCredentialsHelper::FetchResult&
ActorLoginGetCredentialsHelper::FetchResult::operator=(FetchResult&&) = default;

ActorLoginGetCredentialsHelper::FetchResult::~FetchResult() = default;

void ActorLoginGetCredentialsHelper::OnAllFetchesCompleted(
    std::vector<FetchResult> results) {
  for (const FetchResult& result : results) {
    // To keep the same behavior as the previous implementation, check the
    // global error to return `kFillingNotAllowed` error.
    // TODO(crbug.com/478799141): In the future, we should only report an error
    // if all fetchers return an error. Fetcher-specific errors should be logged
    // in MQLS.
    if (result.status) {
      std::optional<ActorLoginError> error = result.status->GetGlobalError();
      if (error.has_value()) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback_),
                                      base::unexpected(error.value())));
        return;
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_),
                                MergeCredentials(std::move(results))));
}

std::vector<Credential> ActorLoginGetCredentialsHelper::MergeCredentials(
    std::vector<FetchResult> results) {
  // TODO(crbug.com/479392389): Update the logic once we decide how we want to
  // merge. Since fetches are asynchronous, the order of the credentials types
  // is not guaranteed. This doesn't matter until we enable federated
  // credentials fetcher.
  std::vector<Credential> credentials;
  for (FetchResult& result : results) {
    for (Credential& credential : result.credentials) {
      credentials.push_back(std::move(credential));
    }
  }
  return credentials;
}

}  // namespace actor_login
