// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
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

  password_manager::PasswordManagerClient* client() const { return client_; }

  // The methods below discard the current state/data of the object and move it
  // to the specified state.

  // Move to PENDING_PASSWORD_STATE.
  void OnPendingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager);

  // Move to PASSWORD_STORE_CHANGED_BUBBLE_STATE.
  void OnDefaultStoreChanged(
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

  // Move to SAVE_CONFIRMATION_STATE.
  void OnAutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager);

  // Move to |state|. Updates local_credentials_forms_ to contain pending
  // credentials.|form_to_update| will be excluded from the confirmation
  // management bubble as it contains outdated data.
  void OnSubmittedGeneratedPassword(
      password_manager::ui::State state,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager,
      password_manager::PasswordForm form_to_update =
          password_manager::PasswordForm());

  // Move to MANAGE_STATE or INACTIVE_STATE for PSL matched passwords.
  // |password_forms| contains best matches from the password store for the
  // form which was autofilled, |origin| is an origin of the form which was
  // autofilled. In addition, |federated_matches|, contains stored federated
  // credentials, if any, to show to the user as well.
  void OnPasswordAutofilled(
      base::span<const password_manager::PasswordForm> password_forms,
      url::Origin origin,
      base::span<const password_manager::PasswordForm> federated_matches);

  // Move to INACTIVE_STATE.
  void OnInactive();

  // Move to KEYCHAIN_ERROR_STATE.
  void OnKeychainError();

  // Move to PASSKEY_SAVED_CONFIRMATION_STATE. Stores whether GPM pin was
  // created in the same flow.
  void OnPasskeySaved(bool gpm_pin_created);

  // Move to PASSKEY_DELETED_CONFIRMATION_STATE.
  void OnPasskeyDeleted();

  // Move to PASSKEY_UPDATED_CONFIRMATION_STATE.
  void OnPasskeyUpdated();

  // Move to PASSKEY_NOT_ACCEPTED_STATE.
  void OnPasskeyNotAccepted();

  // Move to MOVE_CREDENTIAL_AFTER_LOG_IN_STATE. Triggers a bubble to move the
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

  // Move to MANAGE_STATE with initial credential to show its details.
  void OpenPasswordDetailsBubble(const password_manager::PasswordForm& form);

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

  password_manager::PasswordForm* selected_password() const {
    return selected_password_.get();
  }
  void set_selected_password(
      std::unique_ptr<password_manager::PasswordForm> form) {
    selected_password_ = std::move(form);
  }
  void clear_selected_password() { selected_password_.reset(); }

  const std::optional<password_manager::PasswordForm>&
  single_credential_mode_credential() const {
    return single_credential_mode_credential_;
  }

  bool auth_for_account_storage_opt_in_failed() const {
    return auth_for_account_storage_opt_in_failed_;
  }
  void set_auth_for_account_storage_opt_in_failed(bool failed) {
    auth_for_account_storage_opt_in_failed_ = failed;
  }

  bool gpm_pin_created_during_recent_passkey_creation() const {
    return gpm_pin_created_during_recent_passkey_creation_;
  }

  // Current local forms. ManagePasswordsState is responsible for the forms.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  GetCurrentForms() const {
    return local_credentials_forms_;
  }

  void ClearSingleCredentialModeCredential() {
    single_credential_mode_credential_ = std::nullopt;
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

  // Contains password selected for moving to the account.
  std::unique_ptr<password_manager::PasswordForm> selected_password_;

  // The credential for the bubble in the single credential mode.
  std::optional<password_manager::PasswordForm>
      single_credential_mode_credential_;

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
  raw_ptr<password_manager::PasswordManagerClient, AcrossTasksDanglingUntriaged>
      client_;

  // Whether the last attempt to authenticate to opt-in using password account
  // storage failed.
  bool auth_for_account_storage_opt_in_failed_ = false;

  // Whether GPM pin was created in the same flow as recent passkey creation.
  bool gpm_pin_created_during_recent_passkey_creation_ = false;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
