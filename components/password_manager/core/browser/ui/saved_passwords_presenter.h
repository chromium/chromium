// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_

#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/string_piece_forward.h"
#include "components/password_manager/core/browser/password_form_forward.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

// This interface provides a way for clients to obtain a list of all saved
// passwords and register themselves as observers for changes. In contrast to
// simply registering oneself as an observer of a password store directly, this
// class possibly responds to changes in multiple password stores, such as the
// local and account store used for passwords for butter.
// Furthermore, this class exposes a direct mean to edit a password, and
// notifies its observers about this event. An example use case for this is the
// bulk check settings page, where an edit operation in that page should result
// in the new password to be checked, whereas other password edit operations
// (such as visiting a change password form and then updating the password in
// Chrome) should not trigger a check.
class SavedPasswordsPresenter : public PasswordStore::Observer,
                                public PasswordStoreConsumer {
 public:
  using SavedPasswordsView = base::span<const PasswordForm>;

  // Observer interface. Clients can implement this to get notified about
  // changes to the list of saved passwords or if a given password was edited
  // Clients can register and de-register themselves, and are expected to do so
  // before the presenter gets out of scope.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer when a password is edited or the list of saved
    // passwords changed.
    //
    // OnEdited() will be invoked synchronously if EditPassword() is invoked
    // with a password that was present in |passwords_|.
    // |password.password_value| will be equal to |new_password| in this case.
    virtual void OnEdited(const PasswordForm& password) {}
    // OnSavedPasswordsChanged() gets invoked asynchronously after a change to
    // the underlying password store happens. This might be due to a call to
    // EditPassword(), but can also happen if passwords are added or removed due
    // to other reasons.
    virtual void OnSavedPasswordsChanged(SavedPasswordsView passwords) {}
  };

  explicit SavedPasswordsPresenter(
      scoped_refptr<PasswordStore> profile_store,
      scoped_refptr<PasswordStore> account_store = nullptr);
  ~SavedPasswordsPresenter() override;

  // Initializes the presenter and makes it issue the first request for all
  // saved passwords.
  void Init();

  // Tries to edit |password|. After checking whether |form| is present in
  // |passwords_|, this will ask the password store to change the underlying
  // password_value to |new_password| in case it was found. This will also
  // notify clients that an edit event happened in case |form| was present
  // in |passwords_|.
  bool EditPassword(const PasswordForm& form, base::string16 new_password);

  // Returns a list of the currently saved credentials.
  SavedPasswordsView GetSavedPasswords() const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // PasswordStore::Observer
  void OnLoginsChanged(const PasswordStoreChangeList& changes) override;
  void OnLoginsChangedIn(PasswordStore* store,
                         const PasswordStoreChangeList& changes) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStore* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Notify observers about changes in the compromised credentials.
  void NotifyEdited(const PasswordForm& password);
  void NotifySavedPasswordsChanged();

  // The password stores containing the saved passwords.
  scoped_refptr<PasswordStore> profile_store_;
  scoped_refptr<PasswordStore> account_store_;

  // Cache of the most recently obtained saved passwords. Profile store
  // passwords are always stored first, and then account store passwords if any.
  std::vector<PasswordForm> passwords_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
