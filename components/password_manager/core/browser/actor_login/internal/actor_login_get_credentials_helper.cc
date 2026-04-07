// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/barrier_callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics_helper_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace actor_login {

ActorLoginGetCredentialsHelper::ActorLoginGetCredentialsHelper(
    std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers,
    ActorLoginMetricsHelperInterface* metrics_helper,
    CredentialsOrErrorReply callback)
    : fetchers_(std::move(fetchers)),
      metrics_helper_(metrics_helper),
      callback_(std::move(callback)) {
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
           ActorLoginCredentialsFetcher::Status status) {
          barrier.Run({std::move(credentials), status});
        },
        barrier_callback));
  }
}

ActorLoginGetCredentialsHelper::~ActorLoginGetCredentialsHelper() = default;

ActorLoginGetCredentialsHelper::FetchResult::FetchResult() = default;

ActorLoginGetCredentialsHelper::FetchResult::FetchResult(
    std::vector<Credential> credentials,
    ActorLoginCredentialsFetcher::Status status)
    : credentials(std::move(credentials)), status(std::move(status)) {}

ActorLoginGetCredentialsHelper::FetchResult::FetchResult(FetchResult&&) =
    default;

ActorLoginGetCredentialsHelper::FetchResult&
ActorLoginGetCredentialsHelper::FetchResult::operator=(FetchResult&&) = default;

ActorLoginGetCredentialsHelper::FetchResult::~FetchResult() = default;

void ActorLoginGetCredentialsHelper::OnAllFetchesCompleted(
    std::vector<FetchResult> results) {
  bool fetched_credentials = std::ranges::any_of(
      results,
      [](const FetchResult& result) { return !result.credentials.empty(); });

  if (!fetched_credentials) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  GetErrorOrNoCredentials(std::move(results))));
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_),
                                MergeCredentials(std::move(results))));
}

CredentialsOrError ActorLoginGetCredentialsHelper::GetErrorOrNoCredentials(
    std::vector<FetchResult> results) {
  for (const FetchResult& result : results) {
    if (result.status ==
        ActorLoginCredentialsFetcher::Status::kFillingNotAllowed) {
      return base::unexpected(ActorLoginError::kFillingNotAllowed);
    }
  }
  return std::vector<Credential>();
}

std::vector<Credential> ActorLoginGetCredentialsHelper::MergeCredentials(
    std::vector<FetchResult> results) {
  std::vector<Credential> flattened_credentials;
  for (FetchResult& result : results) {
    flattened_credentials.insert(
        flattened_credentials.end(),
        std::make_move_iterator(result.credentials.begin()),
        std::make_move_iterator(result.credentials.end()));
  }

  // Federated credentials have higher priority than password credentials. We
  // need to keep the order of the credentials with the same type as the
  // fetchers already rank them.
  std::stable_partition(
      flattened_credentials.begin(), flattened_credentials.end(),
      [](const Credential& c) { return c.type == CredentialType::kFederated; });

  std::vector<Credential> credentials;
  absl::flat_hash_map<std::u16string, size_t> username_to_index;
  // Remove duplicates by only keeping the first credential with a given
  // username. This way we keep the highest priority credential (federated).
  for (Credential& credential : flattened_credentials) {
    auto it = username_to_index.find(credential.username);
    if (it == username_to_index.end()) {
      username_to_index.emplace(credential.username, credentials.size());
      credentials.emplace_back(std::move(credential));
    } else {
      metrics_helper_->RecordDeduplicationOccurred(true);
      credentials[it->second].has_persistent_permission |=
          credential.has_persistent_permission;
    }
  }

  const int permission_count = std::ranges::count_if(
      credentials, &Credential::has_persistent_permission);

  if (permission_count == 1) {
    Credential approved_credential = *std::ranges::find_if(
        credentials, &Credential::has_persistent_permission);
    return {approved_credential};
  }

  // We treat multiple credentials with permission as a merge conflict. Discard
  // all permissions to allow the user to resolve the conflict.
  // TODO(crbug.com/486089293): Also discard the permission in storage.
  if (permission_count > 1) {
    for (Credential& credential : credentials) {
      credential.has_persistent_permission = false;
    }
  }

  return credentials;
}

}  // namespace actor_login
