// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordFormManagerForUI;
class PasswordManagerClient;
}  // namespace password_manager

// ManagePasswordsState keeps the current state for ManagePasswordsUIController
// as well as up-to-date data for this state.
class ManagePasswordsState {
 public:
  using CredentialsCallback =
      base::OnceCallback<void(const password_manager::PasswordForm*)>;

  ManagePasswordsState();

  ManagePasswordsState(const ManagePasswordsState&) = delete;
  ManagePasswordsState& operator=(const ManagePasswordsState&) = delete;

  ~ManagePasswordsState();

  // The embedder of this class has to set the client for logging.
  void set_client(password_manager::PasswordManagerClient* client) {
    client_ = client;
  }

  password_manager::PasswordManagerClient* client() { return client_; }

  // The methods below discard the current state/data of the object and move it
  // to the specified state.

  // Move to PENDING_PASSWORD_STATE.
  void OnPendingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager);

  // Move to PENDING_PASSWORD_UPDATE_STATE.
  void OnUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager);

  // Move to CREDENTIAL_REQUEST_STATE.
  void OnRequestCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials,
      const url::Origin& origin);

  // Move to AUTO_SIGNIN_STATE. |local_forms| can't be empty.
  void OnAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin);

  // Move to CONFIRMATION_STATE.
  void OnAutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager);

  // Move to MANAGE_STATE or INACTIVE_STATE for PSL matched passwords.
  // |password_forms| contains best matches from the password store for the
  // form which was autofilled, |origin| is an origin of the form which was
  // autofilled. In addition, |federated_matches|, if not null, contains stored
  // federated credentials to show to the user as well.
  void OnPasswordAutofilled(
      const std::vector<const password_manager::PasswordForm*>& password_forms,
      url::Origin origin,
      const std::vector<const password_manager::PasswordForm*>*
          federated_matches);

  // Move to INACTIVE_STATE.
  void OnInactive();

  // Move to CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE. Triggers a bubble to move the
  // just submitted form to the user's account store.
  void OnPasswordMovable(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move);

  // Moves the object to |state| without resetting the internal data. Allowed:
  // * -> MANAGE_STATE
  // * -> PASSWORD_UPDATED_*
  void TransitionToState(password_manager::ui::State state);

  // Updates the internal state applying |changes|.
  void ProcessLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes);

  void ProcessUnsyncedCredentialsWillBeDeleted(
      std::vector<password_manager::PasswordForm> unsynced_credentials);

  // Called when the user chooses a credential. |form| is passed to the
  // credentials callback. Method should be called in the
  // CREDENTIAL_REQUEST_STATE state.
  void ChooseCredential(const password_manager::PasswordForm* form);

  password_manager::ui::State state() const { return state_; }
  const std::vector<password_manager::PasswordForm>& unsynced_credentials()
      const {
    return unsynced_credentials_;
  }
  const url::Origin& origin() const { return origin_; }
  password_manager::PasswordFormManagerForUI* form_manager() const {
    return form_manager_.get();
  }
  const CredentialsCallback& credentials_callback() {
    return credentials_callback_;
  }
  void set_credentials_callback(CredentialsCallback callback) {
    credentials_callback_ = std::move(callback);
  }

  bool auth_for_account_storage_opt_in_failed() const {
    return auth_for_account_storage_opt_in_failed_;
  }
  void set_auth_for_account_storage_opt_in_failed(bool failed) {
    auth_for_account_storage_opt_in_failed_ = failed;
  }

  // Current local forms. ManagePasswordsState is responsible for the forms.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  GetCurrentForms() const {
    return local_credentials_forms_;
  }

 private:
  // Removes all the PasswordForms stored in this object.
  void ClearData();

  // Adds |form| to the internal state if it's relevant.
  bool AddForm(const password_manager::PasswordForm& form);

  void SetState(password_manager::ui::State state);

  // The origin of the current page for which the state is stored. It's used to
  // determine which PasswordStore changes are applicable to the internal state.
  url::Origin origin_;

  // Contains the password that was submitted.
  std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager_;

  // Contains all the current forms.
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials_forms_;

  // Contains any non synced credentials.
  std::vector<password_manager::PasswordForm> unsynced_credentials_;

  // A callback to be invoked when user selects a credential.
  CredentialsCallback credentials_callback_;

  // The current state of the password manager UI.
  password_manager::ui::State state_;

  // The client used for logging.
  raw_ptr<password_manager::PasswordManagerClient, DanglingUntriaged> client_;

  // Whether the last attempt to authenticate to opt-in using password account
  // storage failed.
  bool auth_for_account_storage_opt_in_failed_ = false;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
