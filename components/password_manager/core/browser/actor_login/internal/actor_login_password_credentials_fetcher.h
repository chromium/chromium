// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PASSWORD_CREDENTIALS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PASSWORD_CREDENTIALS_FETCHER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credentials_fetcher.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "url/origin.h"

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerInterface;
}  // namespace password_manager

namespace actor_login {

class ActorLoginFormFinder;

// Actor Login fetcher implementation for password credentials.
class ActorLoginPasswordCredentialsFetcher
    : public ActorLoginCredentialsFetcher,
      public password_manager::FormFetcher::Consumer {
 public:
  // Status specific to password credentials fetching.
  class Status : public ActorLoginCredentialsFetcher::Status {
   public:
    enum class Outcome {
      kSuccess,
      kFillingNotAllowed,
    };

    explicit Status(Outcome outcome) : outcome_(outcome) {}

    std::optional<ActorLoginError> GetGlobalError() const override;

    Outcome outcome() const { return outcome_; }

   private:
    Outcome outcome_;
  };

  ActorLoginPasswordCredentialsFetcher(
      const url::Origin& origin,
      password_manager::PasswordManagerClient* client,
      password_manager::PasswordManagerInterface* password_manager,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger);

  ActorLoginPasswordCredentialsFetcher(
      const ActorLoginPasswordCredentialsFetcher&) = delete;
  ActorLoginPasswordCredentialsFetcher& operator=(
      const ActorLoginPasswordCredentialsFetcher&) = delete;

  ~ActorLoginPasswordCredentialsFetcher() override;

  // ActorLoginCredentialsFetcher:
  void Fetch(FetchResultCallback callback) override;

 private:
  void OnEligibleLoginFormManagersRetrieved(
      FormFinderResult form_finder_result);

  // password_manager::FormFetcher::Consumer:
  void OnFetchCompleted() override;

  // Populates data into `get_credentials_logs_` at the end of credential
  // fetching.
  void BuildGetCredentialsOutcome(const std::vector<Credential>& result);

  url::Origin request_origin_;

  FetchResultCallback callback_;

  raw_ptr<password_manager::PasswordManagerInterface> password_manager_ =
      nullptr;

  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  // Logs entry to be given to `mqls_logger` when the request ends.
  optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails
      get_credentials_logs_;

  // Helper class that sends MQLS logs about full actor login attempt
  // (GetCredentials + AttemptLogin). Owned by AttemptLoginTool.
  // TODO(crbug.com/460025687): Use raw_ptr instead.
  base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger_;

  // Used to compute the request duration.
  base::TimeTicks start_time_;

  // Helper object for finding login forms.
  std::unique_ptr<ActorLoginFormFinder> login_form_finder_;

  std::unique_ptr<password_manager::FormFetcher> owned_form_fetcher_;
  // The form fetcher from which credentials will be retrieved. If a
  // `PasswordFormManager` for a sign-in form already exists, this will be a
  // non-owning pointer to its `FormFetcher`. Otherwise, this class will own
  // the `FormFetcher` via `owned_form_fetcher_`.
  raw_ptr<password_manager::FormFetcher> form_fetcher_ = nullptr;

  bool immediately_available_to_login_ = false;

  base::WeakPtrFactory<ActorLoginPasswordCredentialsFetcher> weak_ptr_factory_{
      this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PASSWORD_CREDENTIALS_FETCHER_H_
