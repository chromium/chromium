// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_READER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_READER_H_

#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "components/password_manager/core/browser/insecure_credentials_consumer.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {
// This class is responsible for reading and listening to change in the
// insecure credentials in the underlying password stores.
class InsecureCredentialsReader
    : public PasswordStore::DatabaseInsecureCredentialsObserver,
      public InsecureCredentialsConsumer {
 public:
  using GetInsecureCredentialsCallback =
      base::OnceCallback<void(std::vector<InsecureCredential>)>;
  // Observer interface. Clients can implement this to get notified about
  // changes to the list of insecure credentials. Clients can register and
  // de-register themselves, and are expected to do so before the provider gets
  // out of scope.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInsecureCredentialsChanged(
        const std::vector<InsecureCredential>& insecure_credentials) = 0;
  };

  // |profile_store| cannot be null, and must outlive this class.
  explicit InsecureCredentialsReader(PasswordStore* profile_store,
                                     PasswordStore* account_store = nullptr);
  InsecureCredentialsReader(const InsecureCredentialsReader&) = delete;
  InsecureCredentialsReader& operator=(const InsecureCredentialsReader&) =
      delete;
  ~InsecureCredentialsReader() override;

  void Init();

  // `callback` must outlive this object.
  void GetAllInsecureCredentials(GetInsecureCredentialsCallback callback);

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // PasswordStore::DatabaseInsecureCredentialsObserver:
  void OnInsecureCredentialsChanged() override;
  void OnInsecureCredentialsChangedIn(PasswordStore* store) override;

  // InsecureCredentialsConsumer:
  void OnGetInsecureCredentials(
      std::vector<InsecureCredential> insecure_credentials) override;
  void OnGetInsecureCredentialsFrom(
      PasswordStore* store,
      std::vector<InsecureCredential> insecure_credentials) override;

  // The password stores containing the insecure credentials.
  // |profile_store_| must not be null and must outlive this class.
  PasswordStore* profile_store_;
  PasswordStore* account_store_;

  // Cache of the most recently obtained insecure credentials.
  std::vector<InsecureCredential> insecure_credentials_;

  // A scoped observer for |profile_store_|, and |account_store_| that listens
  // to changes related to InsecureCredentials only.
  base::ScopedMultiSourceObservation<
      PasswordStore,
      PasswordStore::DatabaseInsecureCredentialsObserver,
      &PasswordStore::AddDatabaseInsecureCredentialsObserver,
      &PasswordStore::RemoveDatabaseInsecureCredentialsObserver>
      observed_password_stores_{this};

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
  std::vector<GetInsecureCredentialsCallback>
      get_all_insecure_credentials_callbacks_;

  // Whether we are still waiting for a first response from the profile and
  // account stores.
  bool profile_store_responded_ = false;
  bool account_store_responded_ = false;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_READER_H_
