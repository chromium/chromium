// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list_types.h"
#include "base/strings/string16.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

struct InteractionsStats;

// This is an API for providing stored credentials to PasswordFormManager (PFM),
// so that PFM instances do not have to talk to PasswordStore directly. This
// indirection allows caching of identical requests from PFM on the same origin,
// as well as easier testing (no need to mock the whole PasswordStore when
// testing a PFM).
// TODO(crbug.com/621355): Actually modify the API to support fetching in the
// FormFetcher instance.
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

  virtual ~FormFetcher() = default;

  // Adds |consumer|, which must not be null. If the current state is
  // NOT_WAITING, calls OnFetchCompleted on the consumer immediately. Assumes
  // that |consumer| outlives |this|.
  virtual void AddConsumer(Consumer* consumer) = 0;

  // Call this to stop |consumer| from receiving updates from |this|.
  virtual void RemoveConsumer(Consumer* consumer) = 0;

  // Returns the current state of the FormFetcher
  virtual State GetState() const = 0;

  // Statistics for recent password bubble usage.
  virtual const std::vector<InteractionsStats>& GetInteractionsStats()
      const = 0;

  // Non-federated matches obtained from the backend. Valid only if GetState()
  // returns NOT_WAITING.
  virtual std::vector<const autofill::PasswordForm*> GetNonFederatedMatches()
      const = 0;

  // Federated matches obtained from the backend. Valid only if GetState()
  // returns NOT_WAITING.
  virtual std::vector<const autofill::PasswordForm*> GetFederatedMatches()
      const = 0;

  // Whether there are blacklisted matches in the backend. Valid only if
  // GetState() returns NOT_WAITING.
  virtual bool IsBlacklisted() const = 0;

  // Non-federated matches obtained from the backend that have the same scheme
  // of this form.
  virtual const std::vector<const autofill::PasswordForm*>&
  GetAllRelevantMatches() const = 0;

  // Nonblacklisted matches obtained from the backend.
  virtual const std::vector<const autofill::PasswordForm*>& GetBestMatches()
      const = 0;

  // Pointer to a preferred entry in the vector returned by GetBestMatches().
  virtual const autofill::PasswordForm* GetPreferredMatch() const = 0;

  // Fetches stored matching logins. In addition the statistics is fetched on
  // platforms with the password bubble. This is called automatically during
  // construction and can be called manually later as well to cause an update
  // of the cached credentials.
  virtual void Fetch() = 0;

  // Creates a copy of |*this| with contains the same credentials without the
  // need for calling Fetch().
  virtual std::unique_ptr<FormFetcher> Clone() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormFetcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_
