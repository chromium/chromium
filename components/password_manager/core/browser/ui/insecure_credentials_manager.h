// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_

#include <map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observer.h"
#include "base/timer/elapsed_timer.h"
#include "base/util/type_safety/strong_alias.h"
#include "components/password_manager/core/browser/compromised_credentials_consumer.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_reader.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "url/gurl.h"

namespace password_manager {

class LeakCheckCredential;

enum class InsecureCredentialTypeFlags {
  kSecure = 0,
  // If the credential was leaked by a data breach.
  kCredentialLeaked = 1 << 0,
  // If the credential was reused on a phishing site.
  kCredentialPhished = 1 << 1,
  // If the credential has a weak password.
  kWeakCredential = 1 << 2,
};

constexpr InsecureCredentialTypeFlags operator&(
    InsecureCredentialTypeFlags lhs,
    InsecureCredentialTypeFlags rhs) {
  return static_cast<InsecureCredentialTypeFlags>(static_cast<int>(lhs) &
                                                  static_cast<int>(rhs));
}

constexpr InsecureCredentialTypeFlags operator|(
    InsecureCredentialTypeFlags lhs,
    InsecureCredentialTypeFlags rhs) {
  return static_cast<InsecureCredentialTypeFlags>(static_cast<int>(lhs) |
                                                  static_cast<int>(rhs));
}

constexpr InsecureCredentialTypeFlags& operator|=(
    InsecureCredentialTypeFlags& lhs,
    InsecureCredentialTypeFlags rhs) {
  lhs = lhs | rhs;
  return lhs;
}

// Unsets the bit responsible for the weak credential in the |flag|.
constexpr InsecureCredentialTypeFlags UnsetWeakCredentialTypeFlag(
    InsecureCredentialTypeFlags flag) {
  return static_cast<InsecureCredentialTypeFlags>(
      static_cast<int>(flag) &
      ~(static_cast<int>(InsecureCredentialTypeFlags::kWeakCredential)));
}

// Checks that |flag| contains at least one of compromised types.
constexpr bool IsCompromised(const InsecureCredentialTypeFlags& flag) {
  return (flag & (InsecureCredentialTypeFlags::kCredentialLeaked |
                  InsecureCredentialTypeFlags::kCredentialPhished)) !=
         InsecureCredentialTypeFlags::kSecure;
}

// Checks that |flag| contains weak type.
constexpr bool IsWeak(const InsecureCredentialTypeFlags& flag) {
  return (flag & InsecureCredentialTypeFlags::kWeakCredential) !=
         InsecureCredentialTypeFlags::kSecure;
}

// Simple struct that augments key values of InsecureCredentials and a password.
struct CredentialView {
  CredentialView(std::string signon_realm,
                 GURL url,
                 base::string16 username,
                 base::string16 password);
  // Enable explicit construction from PasswordForm for convenience.
  explicit CredentialView(const PasswordForm& form);
  CredentialView(const CredentialView& credential);
  CredentialView(CredentialView&& credential);
  CredentialView& operator=(const CredentialView& credential);
  CredentialView& operator=(CredentialView&& credential);
  ~CredentialView();

  std::string signon_realm;
  GURL url;
  base::string16 username;
  base::string16 password;
};

// All information needed by UI to represent InsecureCredential. It's a result
// of deduplicating InsecureCredentials to have single entity for phished,
// leaked and weak credentials with latest |create_time|, and after that joining
// with autofill::PasswordForms to get passwords. If the credential is only
// weak, |create_time| will be unset.
struct CredentialWithPassword : CredentialView {
  explicit CredentialWithPassword(const CredentialView& credential);
  explicit CredentialWithPassword(const CompromisedCredentials& credential);

  CredentialWithPassword(const CredentialWithPassword& other);
  CredentialWithPassword(CredentialWithPassword&& other);
  ~CredentialWithPassword();

  CredentialWithPassword& operator=(const CredentialWithPassword& other);
  CredentialWithPassword& operator=(CredentialWithPassword&& other);
  base::Time create_time;
  InsecureCredentialTypeFlags insecure_type =
      InsecureCredentialTypeFlags::kSecure;
};

// Comparator that can compare CredentialView or CredentialsWithPasswords.
struct PasswordCredentialLess {
  bool operator()(const CredentialView& lhs, const CredentialView& rhs) const {
    return std::tie(lhs.signon_realm, lhs.username, lhs.password) <
           std::tie(rhs.signon_realm, rhs.username, rhs.password);
  }
};

// Extra information about InsecureCredentials which is required by UI.
struct CredentialMetadata;

// This class provides clients with saved insecure credentials and possibility
// to save new LeakedCredentials, edit/delete insecure credentials and match
// insecure credentials with corresponding autofill::PasswordForms. It supports
// an observer interface, and clients can register themselves to get notified
// about changes to the list.
class InsecureCredentialsManager
    : public CompromisedCredentialsReader::Observer,
      public SavedPasswordsPresenter::Observer {
 public:
  using CredentialsView = base::span<const CredentialWithPassword>;

  // Observer interface. Clients can implement this to get notified about
  // changes to the list of compromised and weak credentials. Clients can
  // register and de-register themselves, and are expected to do so before the
  // provider gets out of scope.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCompromisedCredentialsChanged(
        CredentialsView credentials) = 0;
    virtual void OnWeakCredentialsChanged() {}
  };

  InsecureCredentialsManager(
      SavedPasswordsPresenter* presenter,
      scoped_refptr<PasswordStore> profile_store,
      scoped_refptr<PasswordStore> account_store = nullptr);
  ~InsecureCredentialsManager() override;

  void Init();

  // Computes weak credentials in a separate thread and then passes the result
  // to OnWeakCheckDone.
  void StartWeakCheck();

  // Marks all saved credentials which have same username & password as
  // compromised.
  void SaveCompromisedCredential(const LeakCheckCredential& credential);

  // Attempts to change the stored password of |credential| to |new_password|.
  // Returns whether the change succeeded.
  bool UpdateCredential(const CredentialView& credential,
                        const base::StringPiece password);

  // Attempts to remove |credential| from the password store. Returns whether
  // the remove succeeded.
  bool RemoveCredential(const CredentialView& credential);

  // Returns a vector of currently compromised credentials.
  std::vector<CredentialWithPassword> GetCompromisedCredentials() const;

  // Returns a vector of currently weak credentials.
  std::vector<CredentialWithPassword> GetWeakCredentials() const;

  // Returns password forms which map to provided insecure credential.
  // In most of the cases vector will have 1 element only.
  SavedPasswordsPresenter::SavedPasswordsView GetSavedPasswordsFor(
      const CredentialView& credential) const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  using CredentialPasswordsMap =
      std::map<CredentialView, CredentialMetadata, PasswordCredentialLess>;

  // Updates |weak_passwords| set and notifies observers that weak credentials
  // were changed.
  void OnWeakCheckDone(base::ElapsedTimer timer_since_weak_check_start,
                       base::flat_set<base::string16> weak_passwords);

  // CompromisedCredentialsReader::Observer:
  void OnCompromisedCredentialsChanged(
      const std::vector<CompromisedCredentials>& compromised_credentials)
      override;

  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      SavedPasswordsPresenter::SavedPasswordsView passwords) override;

  // Notifies observers when compromised credentials have changed.
  void NotifyCompromisedCredentialsChanged();

  // Notifies observers when weak credentials have changed.
  void NotifyWeakCredentialsChanged();

  // Returns the `profile_store_` or `account_store_` if `form` is stored in the
  // profile store of the account store accordingly.
  PasswordStore& GetStoreFor(const PasswordForm& form);

  // A weak handle to the presenter used to join the list of insecure
  // credentials with saved passwords. Needs to outlive this instance.
  SavedPasswordsPresenter* presenter_ = nullptr;

  // The password stores containing the insecure credentials.
  scoped_refptr<PasswordStore> profile_store_;
  scoped_refptr<PasswordStore> account_store_;

  // The reader used to read the compromised credentials from the password
  // stores.
  CompromisedCredentialsReader compromised_credentials_reader_;

  // Cache of the most recently obtained compromised credentials.
  std::vector<CompromisedCredentials> compromised_credentials_;

  // Cache of the most recently obtained weak passwords.
  base::flat_set<base::string16> weak_passwords_;

  // A map that matches CredentialView to corresponding PasswordForms, latest
  // create_type and combined insecure type.
  CredentialPasswordsMap credentials_to_forms_;

  // A scoped observer for |compromised_credentials_reader_| to listen changes
  // related to CompromisedCredentials only.
  ScopedObserver<CompromisedCredentialsReader,
                 CompromisedCredentialsReader::Observer>
      observed_compromised_credentials_reader_{this};

  // A scoped observer for |presenter_|.
  ScopedObserver<SavedPasswordsPresenter, SavedPasswordsPresenter::Observer>
      observed_saved_password_presenter_{this};

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<InsecureCredentialsManager> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_
