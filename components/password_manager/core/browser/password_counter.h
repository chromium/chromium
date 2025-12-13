// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_COUNTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

class PasswordStoreInterface;

// The class calculates and keeps up-to-date the number of autofillable (not
// blocklisted, not federated, password value non-empty) passwords in two
// password stores.
class PasswordCounter : public PasswordStoreConsumer,
                        public KeyedService,
                        public PasswordStoreInterface::Observer {
 public:
  // Observer gets a notification about the update password counter.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    virtual void OnPasswordCounterChanged() = 0;
  };

  PasswordCounter(PasswordStoreInterface* profile_store,
                  PasswordStoreInterface* account_store);
  PasswordCounter(const PasswordCounter&) = delete;
  PasswordCounter& operator=(const PasswordCounter&) = delete;
  ~PasswordCounter() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  size_t autofillable_passwords() const {
    return profile_passwords_ + account_passwords_;
  }

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(PasswordStoreInterface* store,
                       const PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override;

  // Ping the observers that the password counter has changed.
  void NotifyObservers();

  // Number of autofillable passwords in both accounts.
  size_t profile_passwords_ = 0;
  size_t account_passwords_ = 0;

  const scoped_refptr<PasswordStoreInterface> profile_store_;
  const scoped_refptr<PasswordStoreInterface> account_store_;

  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      profile_observer_{this};
  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      account_observer_{this};
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PasswordCounter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_COUNTER_H_
