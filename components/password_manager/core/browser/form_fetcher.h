// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/observer_list_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/signin/public/base/gaia_id_hash.h"

namespace password_manager {

struct InteractionsStats;
struct PasswordForm;

// This is an API for providing stored credentials to PasswordFormManager (PFM),
// so that PFM instances do not have to talk to PasswordStore directly. This
// indirection allows caching of identical requests from PFM on the same origin,
// as well as easier testing (no need to mock the whole PasswordStore when
// testing a PFM).
class FormFetcher {
 public:
  // State of waiting for a response from a PasswordStore. There might be
  // multiple transitions between these states.
  enum class State { WAITING, NOT_WAITING };

  // API to be implemented by classes which want the results from FormFetcher.
  class Consumer : public base::CheckedObserver {
   public:
    // FormFetcher calls this method every time the state changes from WAITING
    // to NOT_WAITING. It is now safe for consumers to call the accessor
    // functions for matches.
    virtual void OnFetchCompleted() = 0;
  };

  FormFetcher() = default;

  FormFetcher(const FormFetcher&) = delete;
  FormFetcher& operator=(const FormFetcher&) = delete;

  virtual ~FormFetcher() = default;

  // Adds |consumer|, which must not be null. If the current state is
  // NOT_WAITING, calls OnFetchCompleted on the consumer immediately. Assumes
  // that |consumer| outlives |this|.
  virtual void AddConsumer(Consumer* consumer) = 0;

  // Call this to stop |consumer| from receiving updates from |this|.
  virtual void RemoveConsumer(Consumer* consumer) = 0;

  // Fetches stored matching logins. In addition the statistics is fetched on
  // platforms with the password bubble. This is called automatically during
  // construction and can be called manually later as well to cause an update
  // of the cached credentials.
  virtual void Fetch() = 0;

  // Returns the current state of the FormFetcher
  virtual State GetState() const = 0;

  // Statistics for recent password bubble usage.
  virtual const std::vector<InteractionsStats>& GetInteractionsStats()
      const = 0;

  // Returns all PasswordForm entries that have insecure features.
  virtual base::span<const PasswordForm> GetInsecureCredentials() const = 0;

  // Non-federated matches obtained from the backend.
  virtual base::span<const PasswordForm> GetNonFederatedMatches() const = 0;

  // Federated matches obtained from the backend.
  virtual base::span<const PasswordForm> GetFederatedMatches() const = 0;

  // Whether there are blocklisted matches in the backend. Valid only if
  // GetState() returns NOT_WAITING.
  virtual bool IsBlocklisted() const = 0;

  // Whether moving the credentials with |username| from the
  // local store to the account store for the user with
  // |destination| GaiaIdHash is blocked. This is relevant only for account
  // store users.
  virtual bool IsMovingBlocked(const signin::GaiaIdHash& destination,
                               const std::u16string& username) const = 0;

  // Non-federated matches obtained from the backend that have the same scheme
  // of this form.
  virtual base::span<const PasswordForm> GetAllRelevantMatches() const = 0;

  // Nonblocklisted matches obtained from the backend.
  virtual base::span<const PasswordForm> GetBestMatches() const = 0;

  // Pointer to a preferred entry in the vector returned by GetBestMatches().
  virtual const PasswordForm* GetPreferredMatch() const = 0;

  // If prefferred match exists, returns its form type. Please note, that
  // `FormFetcher` ignored grouped credentials by default. However, if any
  // grouped credentials are available, this function will return the form type
  // of the potential grouped credential. `GetPreferredMatch` will still return
  // `nullptr` in this case.
  // Returns `std::nullopt` if no credentials were available.
  virtual std::optional<PasswordFormMetricsRecorder::MatchedFormType>
  GetPreferredOrPotentialMatchedFormType() const = 0;

  // Creates a copy of |*this| with contains the same credentials without the
  // need for calling Fetch().
  virtual std::unique_ptr<FormFetcher> Clone() = 0;

  // Returns an error if it occurred during login retrieval from the
  // profile store.
  virtual std::optional<PasswordStoreBackendError> GetProfileStoreBackendError()
      const = 0;

  // Returns an error if it occurred during login retrieval from the
  // account store.
  virtual std::optional<PasswordStoreBackendError> GetAccountStoreBackendError()
      const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_
