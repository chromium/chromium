// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_COUNTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

class PasswordStoreInterface;

// The class calculates and keeps up-to-date the number of autofillable (not
// blocklisted) passwords in two password stores.
class PasswordCounter : public PasswordStoreConsumer,
                        public PasswordStoreInterface::Observer {
 public:
  // Delegate gets a notification about the update password counter.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void OnPasswordCounterChanged() = 0;
  };

  PasswordCounter(PasswordStoreInterface* profile_store,
                  PasswordStoreInterface* account_store,
                  Delegate* delegate);
  PasswordCounter(const PasswordCounter&) = delete;
  PasswordCounter& operator=(const PasswordCounter&) = delete;
  ~PasswordCounter() override;

  size_t profile_passwords() const { return profile_passwords_; }
  size_t account_passwords() const { return account_passwords_; }

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

  // Ping the delegate that the password counter has changed.
  void NotifyDelegate();

  // Number of autofillable passwords in both accounts.
  size_t profile_passwords_ = 0;
  size_t account_passwords_ = 0;

  raw_ptr<Delegate> delegate_ = nullptr;
  scoped_refptr<PasswordStoreInterface> profile_store_;
  scoped_refptr<PasswordStoreInterface> account_store_;

  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      profile_observer_;
  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      account_observer_;

  base::WeakPtrFactory<PasswordCounter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_COUNTER_H_
