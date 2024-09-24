// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/affiliated_group.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace password_manager {

namespace metrics_util {
enum class MoveToAccountStoreTrigger;
}

class PasswordUndoHelper;
class PasswordsGrouper;

// This interface provides a way for clients to obtain a list of all saved
// passwords and register themselves as observers for changes. In contrast to
// simply registering oneself as an observer of a password store directly, this
// class possibly responds to changes in multiple password stores, such as the
// local and account store used for passwords for account store users.
// Furthermore, this class exposes a direct mean to edit a password, and
// notifies its observers about this event. An example use case for this is the
// bulk check settings page, where an edit operation in that page should result
// in the new password to be checked, whereas other password edit operations
// (such as visiting a change password form and then updating the password in
// Chrome) should not trigger a check.
class SavedPasswordsPresenter : public PasswordStoreInterface::Observer,
                                public webauthn::PasskeyModel::Observer,
                                public PasswordStoreConsumer {
 public:
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
    // with a password that was present in cache.
    // |password.password_value| will be equal to |new_password| in this case.
    virtual void OnEdited(const CredentialUIEntry& password) {}
    // OnSavedPasswordsChanged() gets invoked asynchronously after a change to
    // the underlying password store happens. This might be due to a call to
    // EditPassword(), but can also happen if passwords are added or removed due
    // to other reasons.
    virtual void OnSavedPasswordsChanged(
        const PasswordStoreChangeList& changes) {}
  };

  // Result of EditSavedCredentials.
  enum class EditResult {
    // Some credentials were successfully updated.
    kSuccess,
    // New credential matches the old one so nothing was changed.
    kNothingChanged,
    // Credential couldn't be found in the store.
    kNotFound,
    // Credentials with the same username and sign on realm already exists.
    kAlreadyExisits,
    // Password was empty.
    kEmptyPassword,
    kMaxValue = kEmptyPassword,
  };

  // Result of AddCredentialsCallback.
  enum class AddResult {
    // Credential is expected to be added successfully.
    kSuccess,
    // Credential is invalid.
    kInvalid,
    // Credential (with the same username, realm, and password) already exists
    // in the profile or/and account store.
    kExactMatch,
    // Credential with the same username and realm already exists in the profile
    // store.
    kConflictInProfileStore,
    // Credential with the same username and realm already exists in the account
    // store.
    kConflictInAccountStore,
    // Credential with the same username and realm already exists in both
    // profile and account stores.
    kConflictInProfileAndAccountStore,
    kMaxValue = kConflictInProfileAndAccountStore,
  };

  using AddCredentialsCallback = base::OnceClosure;
  using DuplicatePasswordsMap = std::multimap<std::string, PasswordForm>;

  SavedPasswordsPresenter(affiliations::AffiliationService* affiliation_service,
                          scoped_refptr<PasswordStoreInterface> profile_store,
                          scoped_refptr<PasswordStoreInterface> account_store,
                          webauthn::PasskeyModel* passkey_store = nullptr);
  ~SavedPasswordsPresenter() override;

  SavedPasswordsPresenter(const SavedPasswordsPresenter&) = delete;
  SavedPasswordsPresenter& operator=(const SavedPasswordsPresenter&) = delete;

  // Initializes the presenter and makes it issue the first request for all
  // saved passwords.
  void Init(base::OnceClosure completion_callback = base::DoNothing());

  // Returns whether there are ongoing fetch requests to credential stores.
  bool IsWaitingForPasswordStore() const;

  // Removes the credential and all its duplicates from the store.
  bool RemoveCredential(const CredentialUIEntry& credential);

  // Cancels the last removal operation.
  void UndoLastRemoval();

  // Adds the |credential| to the specified store. Returns true if the password
  // was added, false if |credential|'s data is not valid (invalid url/empty
  // password), or an entry with such signon_realm and username already exists
  // in any (profile or account) store.
  bool AddCredential(const CredentialUIEntry& credential,
                     password_manager::PasswordForm::Type type =
                         password_manager::PasswordForm::Type::kManuallyAdded);

  // Adds |credentials| to the specified store.
  // Credentials are expected to be valid according to `GetExpectedAddResult`
  // and they should all belong to the same Password Store.
  //
  // NOTE: Informing observers of credentials belonging to mixed types of stores
  // is not supported.
  //
  // For a single credential the behaviour is identical to AddCredential method.
  void AddCredentials(const std::vector<CredentialUIEntry>& credentials,
                      password_manager::PasswordForm::Type type,
                      AddCredentialsCallback completion);

  // Deletes all saved credentials: passwords, passkeys, blocked entries.
  void DeleteAllData(base::OnceCallback<void(bool)> success_callback);

  // Updates all matching password forms in |password_forms|.
  // |completion| will be run after the forms are updated.
  //
  // NOTE: Updates to different password stores is not supported and hence all
  // forms in |password_forms| must belong to the same store.
  void UpdatePasswordForms(const std::vector<PasswordForm>& password_forms,
                           base::OnceClosure completion = base::DoNothing());

  // Modifies all the saved credentials matching |original_credential| to
  // |updated_credential|. Only username, password, notes, display names and
  // password issues are modifiable.
  EditResult EditSavedCredentials(const CredentialUIEntry& original_credential,
                                  const CredentialUIEntry& updated_credential);

  // Moves credential to an account by deleting them from profile password store
  // and adding them to the account password store. `trigger` is used to record
  // per entry point metrics.
  void MoveCredentialsToAccount(
      const std::vector<CredentialUIEntry>& credentials,
      metrics_util::MoveToAccountStoreTrigger trigger);

  // Returns a list of unique passwords which includes normal credentials,
  // federated credentials, passkeys, and blocked forms. If a same form is
  // present both on account and profile stores it will be represented as a
  // single entity. Uniqueness is determined using site name, username,
  // password. For Android credentials package name is also taken into account
  // and for Federated credentials federation origin.
  std::vector<CredentialUIEntry> GetSavedCredentials() const;

  // Returns a list of affiliated groups for the Password Manager.
  std::vector<AffiliatedGroup> GetAffiliatedGroups();

  // Returns a list of saved passwords (excluding blocked and federated
  // credentials).
  std::vector<CredentialUIEntry> GetSavedPasswords() const;

  // Returns a list of sites blocked by users for the Password Manager.
  std::vector<CredentialUIEntry> GetBlockedSites();

  // Returns PasswordForms corresponding to |credential|.
  std::vector<PasswordForm> GetCorrespondingPasswordForms(
      const CredentialUIEntry& credential) const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(PasswordStoreInterface* store,
                       const PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override;

  // PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStoreInterface* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Notify observers about changes in the compromised credentials.
  void NotifyEdited(const CredentialUIEntry& password);
  void NotifySavedPasswordsChanged(const PasswordStoreChangeList& changes);

  // Returns the expected result for adding |credential|, looks for
  // missing/invalid fields and checks if the credential already exists in the
  // memory cache.
  AddResult GetExpectedAddResult(const CredentialUIEntry& credential) const;

  // Returns the `profile_store_` or `account_store_` if `form` is stored in
  // the profile store or the account store accordingly. This function should
  // be used only for credential stored in a single store.
  PasswordStoreInterface& GetStoreFor(const PasswordForm& form);

  // Try to unblocklist in both stores.If credentials don't
  // exist, the unblocklist operation is a no-op.
  void UnblocklistBothStores(const CredentialUIEntry& credential);

  // Helper functions to update local cache of PasswordForms.
  void RemoveForms(const std::vector<PasswordForm>& forms);
  void AddForms(const std::vector<PasswordForm>& forms,
                base::OnceClosure completion);

  // Collects credentials and groups them if there are no pending store updates.
  void MaybeGroupCredentials(base::OnceClosure completion);

  // Edits an existing passkey.
  EditResult EditPasskey(const CredentialUIEntry& updated_credential);

  // Edits an existing password.
  EditResult EditPassword(const CredentialUIEntry& original_credential,
                          const CredentialUIEntry& updated_credential);

  // The password stores containing the saved passwords.
  scoped_refptr<PasswordStoreInterface> profile_store_;
  scoped_refptr<PasswordStoreInterface> account_store_;

  // Store containing account passkeys. This may be null if the feature is
  // disabled.
  raw_ptr<webauthn::PasskeyModel> passkey_store_;

  // The number of stores from which no updates have been received yet.
  int pending_store_updates_ = 0;

  std::unique_ptr<PasswordUndoHelper> undo_helper_;

  // Helper object which groups passwords based on information provided by the
  // affiliation service.
  std::unique_ptr<PasswordsGrouper> passwords_grouper_;

  // Structure used to deduplicate list of passwords.
  DuplicatePasswordsMap sort_key_to_password_forms_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::OnceClosure init_completion_callback_;

  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      profile_store_observation_{this};
  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      account_store_observation_{this};
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      passkey_store_observation_{this};

  base::WeakPtrFactory<SavedPasswordsPresenter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
