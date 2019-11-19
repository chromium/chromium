// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordFormManagerForUI;
class PasswordManagerClient;
}


// ManagePasswordsState keeps the current state for ManagePasswordsUIController
// as well as up-to-date data for this state.
class ManagePasswordsState {
 public:
  using CredentialsCallback =
      base::Callback<void(const autofill::PasswordForm*)>;

  ManagePasswordsState();
  ~ManagePasswordsState();

  // The embedder of this class has to set the client for logging.
  void set_client(password_manager::PasswordManagerClient* client) {
    client_ = client;
  }

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
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
      const GURL& origin);

  // Move to AUTO_SIGNIN_STATE. |local_forms| can't be empty.
  void OnAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin);

  // Move to CONFIRMATION_STATE.
  void OnAutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager);

  // Move to MANAGE_STATE or INACTIVE_STATE for PSL matched passwords.
  // |password_forms| contains best matches from the password store for the
  // form which was autofilled, |origin| is an origin of the form which was
  // autofilled. In addition, |federated_matches|, if not null, contains stored
  // federated credentials to show to the user as well.
  void OnPasswordAutofilled(
      const std::vector<const autofill::PasswordForm*>& password_forms,
      GURL origin,
      const std::vector<const autofill::PasswordForm*>* federated_matches);

  // Move to INACTIVE_STATE.
  void OnInactive();

  // Moves the object to |state| without resetting the internal data. Allowed:
  // * -> MANAGE_STATE
  void TransitionToState(password_manager::ui::State state);

  // Updates the internal state applying |changes|.
  void ProcessLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes);

  // Called when the user chooses a credential. |form| is passed to the
  // credentials callback. Method should be called in the
  // CREDENTIAL_REQUEST_STATE state.
  void ChooseCredential(const autofill::PasswordForm* form);

  password_manager::ui::State state() const { return state_; }
  const GURL& origin() const { return origin_; }
  password_manager::PasswordFormManagerForUI* form_manager() const {
    return form_manager_.get();
  }
  const CredentialsCallback& credentials_callback() {
    return credentials_callback_;
  }
  void set_credentials_callback(const CredentialsCallback& callback) {
    credentials_callback_ = callback;
  }

  // Current local forms. ManagePasswordsState is responsible for the forms.
  const std::vector<std::unique_ptr<autofill::PasswordForm>>& GetCurrentForms()
      const {
    return local_credentials_forms_;
  }

 private:
  // Removes all the PasswordForms stored in this object.
  void ClearData();

  // Adds |form| to the internal state if it's relevant.
  bool AddForm(const autofill::PasswordForm& form);

  void SetState(password_manager::ui::State state);

  // The origin of the current page for which the state is stored. It's used to
  // determine which PasswordStore changes are applicable to the internal state.
  GURL origin_;

  // Contains the password that was submitted.
  std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager_;

  // Contains all the current forms.
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials_forms_;

  // A callback to be invoked when user selects a credential.
  CredentialsCallback credentials_callback_;

  // The current state of the password manager UI.
  password_manager::ui::State state_;

  // The client used for logging.
  password_manager::PasswordManagerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsState);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
