// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_READER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_READER_H_

#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "components/password_manager/core/browser/compromised_credentials_consumer.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {
// This class is responsible for reading and listening to change in the
// compormised credentials in the underlying password stores.
class CompromisedCredentialsReader
    : public PasswordStore::DatabaseCompromisedCredentialsObserver,
      public CompromisedCredentialsConsumer {
 public:
  using GetCompromisedCredentialsCallback =
      base::OnceCallback<void(std::vector<CompromisedCredentials>)>;
  // Observer interface. Clients can implement this to get notified about
  // changes to the list of compromised credentials. Clients can register and
  // de-register themselves, and are expected to do so before the provider gets
  // out of scope.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCompromisedCredentialsChanged(
        const std::vector<CompromisedCredentials>& compromised_credentials) = 0;
  };

  // |profile_store| cannot be null, and must outlive this class.
  explicit CompromisedCredentialsReader(PasswordStore* profile_store,
                                        PasswordStore* account_store = nullptr);
  CompromisedCredentialsReader(const CompromisedCredentialsReader&) = delete;
  CompromisedCredentialsReader& operator=(const CompromisedCredentialsReader&) =
      delete;
  ~CompromisedCredentialsReader() override;

  void Init();

  // `callback` must outlive this object.
  void GetAllCompromisedCredentials(GetCompromisedCredentialsCallback callback);

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // PasswordStore::DatabaseCompromisedCredentialsObserver:
  void OnCompromisedCredentialsChanged() override;
  void OnCompromisedCredentialsChangedIn(PasswordStore* store) override;

  // CompromisedCredentialsConsumer:
  void OnGetCompromisedCredentials(
      std::vector<CompromisedCredentials> compromised_credentials) override;
  void OnGetCompromisedCredentialsFrom(
      PasswordStore* store,
      std::vector<CompromisedCredentials> compromised_credentials) override;

  // The password stores containing the compromised credentials.
  // |profile_store_| must not be null and must outlive this class.
  PasswordStore* profile_store_;
  PasswordStore* account_store_;

  // Cache of the most recently obtained compromised credentials.
  std::vector<CompromisedCredentials> compromised_credentials_;

  // A scoped observer for |profile_store_|, and |account_store_| that listens
  // to changes related to CompromisedCredentials only.
  ScopedObserver<PasswordStore,
                 PasswordStore::DatabaseCompromisedCredentialsObserver,
                 &PasswordStore::AddDatabaseCompromisedCredentialsObserver,
                 &PasswordStore::RemoveDatabaseCompromisedCredentialsObserver>
      observed_password_store_{this};

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
  std::vector<GetCompromisedCredentialsCallback>
      get_all_compromised_credentials_callbacks_;

  // Whether we are still waiting for a first response from the profile and
  // account stores.
  bool profile_store_responded_ = false;
  bool account_store_responded_ = false;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_READER_H_
