// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "url/gurl.h"

namespace password_manager {

struct CredentialUIEntry;
class LeakCheckCredential;

enum class InsecureCredentialTypeFlags {
  kSecure = 0,
  // If the credential was leaked by a data breach.
  kCredentialLeaked = 1 << 0,
  // If the credential was reused on a phishing site.
  kCredentialPhished = 1 << 1,
  // If the credential has a weak password.
  kWeakCredential = 1 << 2,
  // If the credentials has the same password as another credentials.
  kReusedCredential = 1 << 3,
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

// Unsets the bits responsible for the reused and weak credential in the |flag|.
constexpr InsecureCredentialTypeFlags UnsetWeakAndReusedCredentialTypeFlags(
    InsecureCredentialTypeFlags flag) {
  return static_cast<InsecureCredentialTypeFlags>(
      static_cast<int>(flag) &
      ~(static_cast<int>(InsecureCredentialTypeFlags::kWeakCredential |
                         InsecureCredentialTypeFlags::kReusedCredential)));
}

// Checks that |flag| contains at least one of insecure types.
constexpr bool IsInsecure(const InsecureCredentialTypeFlags& flag) {
  return (flag & (InsecureCredentialTypeFlags::kCredentialLeaked |
                  InsecureCredentialTypeFlags::kCredentialPhished)) !=
         InsecureCredentialTypeFlags::kSecure;
}

// Checks that |flag| contains weak type.
constexpr bool IsWeak(const InsecureCredentialTypeFlags& flag) {
  return (flag & InsecureCredentialTypeFlags::kWeakCredential) !=
         InsecureCredentialTypeFlags::kSecure;
}

// Simple struct that augments key values of InsecureCredential and a password.
struct CredentialView {
  CredentialView(std::string signon_realm,
                 GURL url,
                 std::u16string username,
                 std::u16string password,
                 base::Time last_used_time);
  // Enable explicit construction from PasswordForm for convenience.
  explicit CredentialView(const PasswordForm& form);
  CredentialView(const CredentialView& credential);
  CredentialView(CredentialView&& credential);
  CredentialView& operator=(const CredentialView& credential);
  CredentialView& operator=(CredentialView&& credential);
  ~CredentialView();

  std::string signon_realm;
  GURL url;
  std::u16string username;
  std::u16string password;
  base::Time last_used_time;
};

// All information needed by UI to represent InsecureCredential. It's a result
// of deduplicating InsecureCredential to have single entity for phished,
// leaked and weak credentials with latest |create_time|, and after that joining
// with autofill::PasswordForms to get passwords. If the credential is only
// weak, |create_time| will be unset.
struct CredentialWithPassword : CredentialView {
  explicit CredentialWithPassword(const CredentialView& credential);
  explicit CredentialWithPassword(const InsecureCredential& credential);
  CredentialWithPassword(const CredentialWithPassword& other);
  CredentialWithPassword(CredentialWithPassword&& other);
  ~CredentialWithPassword();

  CredentialWithPassword& operator=(const CredentialWithPassword& other);
  CredentialWithPassword& operator=(CredentialWithPassword&& other);
  base::Time create_time;
  InsecureCredentialTypeFlags insecure_type =
      InsecureCredentialTypeFlags::kSecure;
  IsMuted is_muted{false};
};

// Comparator that can compare CredentialView or CredentialsWithPasswords.
struct PasswordCredentialLess {
  bool operator()(const CredentialView& lhs, const CredentialView& rhs) const {
    return std::tie(lhs.signon_realm, lhs.username, lhs.password) <
           std::tie(rhs.signon_realm, rhs.username, rhs.password);
  }
};

// Extra information about InsecureCredential which is required by UI.
struct CredentialMetadata;

// This class provides clients with saved insecure credentials and possibility
// to save new LeakedCredentials, edit/delete/[un]mute insecure credentials and
// match insecure credentials with corresponding autofill::PasswordForms. It
// supports an observer interface, and clients can register themselves to get
// notified about changes to the list.
class InsecureCredentialsManager : public SavedPasswordsPresenter::Observer {
 public:
  using CredentialsView = base::span<const CredentialWithPassword>;

  // Observer interface. Clients can implement this to get notified about
  // changes to the list of insecure and weak credentials. Clients can register
  // and de-register themselves, and are expected to do so before the provider
  // gets out of scope.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInsecureCredentialsChanged(CredentialsView credentials) = 0;
    virtual void OnWeakCredentialsChanged() {}
  };

  InsecureCredentialsManager(
      SavedPasswordsPresenter* presenter,
      scoped_refptr<PasswordStoreInterface> profile_store,
      scoped_refptr<PasswordStoreInterface> account_store = nullptr);
  ~InsecureCredentialsManager() override;

  void Init();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Computes weak credentials in a separate thread and then passes the result
  // to OnWeakCheckDone.
  void StartWeakCheck(base::OnceClosure on_check_done = base::DoNothing());
#endif

  // Marks all saved credentials which have same username & password as
  // insecure.
  void SaveInsecureCredential(const LeakCheckCredential& credential);

  // Attempts to change the stored password of |credential| to |new_password|.
  // Returns whether the change succeeded.
  bool UpdateCredential(const CredentialView& credential,
                        const base::StringPiece password);

  // Attempts to remove |credential| from the password store. Returns whether
  // the remove succeeded.
  bool RemoveCredential(const CredentialView& credential);

  // Attempts to mute |credential| from the password store.
  // Returns whether the mute succeeded.
  bool MuteCredential(const CredentialUIEntry& credential);

  // Attempts to unmute |credential| from the password store.
  // Returns whether the unmute succeeded.
  bool UnmuteCredential(const CredentialUIEntry& credential);

  // Returns a vector of currently insecure credentials.
  // TODO(crbug.com/1330549): Use CredentialUIEntry only.
  std::vector<CredentialWithPassword> GetInsecureCredentials() const;
  std::vector<CredentialUIEntry> GetInsecureCredentialEntries() const;

  // Returns a vector of currently weak credentials.
  // TODO(crbug.com/1330549): Use CredentialUIEntry only.
  std::vector<CredentialWithPassword> GetWeakCredentials() const;
  std::vector<CredentialUIEntry> GetWeakCredentialEntries() const;

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

  // Recomputes the insecure credentials by making use of information stored in
  // `insecure_credentials_`, `weak_passwords_` and `presenter_`.
  // This does not invoke either `NotifyInsecureCredentialsChanged` or
  // `NotifyWeakCredentialsChanged`, so that it can be used more generally.
  void UpdateInsecureCredentials();

  // Updates |weak_passwords| set and notifies observers that weak credentials
  // were changed.
  void OnWeakCheckDone(base::ElapsedTimer timer_since_weak_check_start,
                       base::flat_set<std::u16string> weak_passwords);

  // SavedPasswordsPresenter::Observer:
  void OnEdited(const PasswordForm& form) override;
  void OnSavedPasswordsChanged(
      SavedPasswordsPresenter::SavedPasswordsView passwords) override;

  // Notifies observers when insecure credentials have changed.
  void NotifyInsecureCredentialsChanged();

  // Notifies observers when weak credentials have changed.
  void NotifyWeakCredentialsChanged();

  // Returns the `profile_store_` or `account_store_` if `form` is stored in the
  // profile store of the account store accordingly.
  PasswordStoreInterface& GetStoreFor(const PasswordForm& form);

  // A weak handle to the presenter used to join the list of insecure
  // credentials with saved passwords. Needs to outlive this instance.
  raw_ptr<SavedPasswordsPresenter> presenter_ = nullptr;

  // The password stores containing the insecure credentials.
  scoped_refptr<PasswordStoreInterface> profile_store_;
  scoped_refptr<PasswordStoreInterface> account_store_;

  // Cache of the most recently obtained insecure credentials.
  std::vector<InsecureCredential> insecure_credentials_;

  // Cache of the most recently obtained weak passwords.
  base::flat_set<std::u16string> weak_passwords_;

  // A map that matches CredentialView to corresponding PasswordForms, latest
  // create_type and combined insecure type.
  CredentialPasswordsMap credentials_to_forms_;

  // A scoped observer for |presenter_|.
  base::ScopedObservation<SavedPasswordsPresenter,
                          SavedPasswordsPresenter::Observer>
      observed_saved_password_presenter_{this};

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<InsecureCredentialsManager> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_
