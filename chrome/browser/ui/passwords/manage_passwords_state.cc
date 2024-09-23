// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_state.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "url/gurl.h"
#include "url/origin.h"

using password_manager::PasswordForm;
using password_manager::PasswordFormManagerForUI;
using password_manager_util::GetMatchType;

namespace {

std::vector<std::unique_ptr<PasswordForm>> DeepCopyNonPSLVector(
    base::span<const PasswordForm> password_forms) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.reserve(password_forms.size());
  for (const PasswordForm& form : password_forms) {
    if (GetMatchType(form) != password_manager_util::GetLoginMatchType::kPSL) {
      result.push_back(std::make_unique<PasswordForm>(form));
    }
  }
  return result;
}

void AppendDeepCopyVector(base::span<const PasswordForm> forms,
                          std::vector<std::unique_ptr<PasswordForm>>* result) {
  result->reserve(result->size() + forms.size());
  for (const password_manager::PasswordForm& form : forms) {
    result->push_back(std::make_unique<PasswordForm>(form));
  }
}

// Updates one form in |forms| that has the same unique key as |updated_form|.
// Returns true if the form was found and updated.
bool UpdateFormInVector(const PasswordForm& updated_form,
                        std::vector<std::unique_ptr<PasswordForm>>* forms) {
  auto it = base::ranges::find_if(
      *forms, [&updated_form](const std::unique_ptr<PasswordForm>& form) {
        return ArePasswordFormUniqueKeysEqual(*form, updated_form);
      });
  if (it != forms->end()) {
    **it = updated_form;
    return true;
  }
  return false;
}

// Removes a form from |forms| that has the same unique key as |form_to_delete|.
// Returns true iff the form was deleted.
bool RemoveFormFromVector(const PasswordForm& form_to_delete,
                          std::vector<std::unique_ptr<PasswordForm>>* forms) {
  auto it = base::ranges::find_if(
      *forms, [&form_to_delete](const std::unique_ptr<PasswordForm>& form) {
        return ArePasswordFormUniqueKeysEqual(*form, form_to_delete);
      });
  if (it != forms->end()) {
    forms->erase(it);
    return true;
  }
  return false;
}

}  // namespace

ManagePasswordsState::ManagePasswordsState()
    : state_(password_manager::ui::INACTIVE_STATE), client_(nullptr) {}

ManagePasswordsState::~ManagePasswordsState() {}

void ManagePasswordsState::OnPendingPassword(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  local_credentials_forms_ =
      DeepCopyNonPSLVector(form_manager_->GetBestMatches());
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);
  origin_ = url::Origin::Create(form_manager_->GetURL());
  SetState(password_manager::ui::PENDING_PASSWORD_STATE);
}

void ManagePasswordsState::OnDefaultStoreChanged(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  // OnPendingPassword() sets the state to PENDING_PASSWORD_STATE, so
  // TransitionToState() needs to be called second.
  OnPendingPassword(std::move(form_manager));
  TransitionToState(password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE);
}

void ManagePasswordsState::OnUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  local_credentials_forms_ =
      DeepCopyNonPSLVector(form_manager_->GetBestMatches());
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);
  origin_ = url::Origin::Create(form_manager_->GetURL());
  SetState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

void ManagePasswordsState::OnRequestCredentials(
    std::vector<std::unique_ptr<PasswordForm>> local_credentials,
    const url::Origin& origin) {
  ClearData();
  local_credentials_forms_ = std::move(local_credentials);
  origin_ = origin;
  SetState(password_manager::ui::CREDENTIAL_REQUEST_STATE);
}

void ManagePasswordsState::OnAutoSignin(
    std::vector<std::unique_ptr<PasswordForm>> local_forms,
    const url::Origin& origin) {
  DCHECK(!local_forms.empty());
  ClearData();
  local_credentials_forms_ = std::move(local_forms);
  origin_ = origin;
  SetState(password_manager::ui::AUTO_SIGNIN_STATE);
}

void ManagePasswordsState::OnAutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  for (const password_manager::PasswordForm& form :
       form_manager_->GetBestMatches()) {
    if (GetMatchType(form) == password_manager_util::GetLoginMatchType::kPSL) {
      continue;
    }
    local_credentials_forms_.push_back(std::make_unique<PasswordForm>(form));
  }
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);
  origin_ = url::Origin::Create(form_manager_->GetURL());
  SetState(password_manager::ui::SAVE_CONFIRMATION_STATE);
}

void ManagePasswordsState::OnSubmittedGeneratedPassword(
    password_manager::ui::State state,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager,
    password_manager::PasswordForm form_to_update) {
  CHECK(state == password_manager::ui::SAVE_CONFIRMATION_STATE ||
        state == password_manager::ui::UPDATE_CONFIRMATION_STATE ||
        state == password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE);
  if (form_manager) {
    ClearData();
    form_manager_ = std::move(form_manager);
  }

  local_credentials_forms_ =
      DeepCopyNonPSLVector(form_manager_->GetBestMatches());
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);

  // When confirmation bubble for the added username is shown, the old
  // credential(without the added username) can be still present in
  // best_matches, if FormFetcher didn't finish fetching passwords yet. It needs
  // to be removed before showing the confirmation bubble.
  std::erase_if(local_credentials_forms_,
                [form_to_update](const std::unique_ptr<PasswordForm>& form) {
                  return ArePasswordFormUniqueKeysEqual(form_to_update, *form);
                });

  // In the confirmation state, a list of saved passwords is displayed, and that
  // list should contain the just added one. This step should be skipped when
  // pending password is already present in the `local_credentials_forms_`. That
  // can happen when this is a confirmation of a password update done via
  // CredentialManager.
  auto it = base::ranges::find_if(
      local_credentials_forms_,
      [this](const std::unique_ptr<PasswordForm>& form) {
        return ArePasswordFormUniqueKeysEqual(
            *form, form_manager_->GetPendingCredentials());
      });
  if (it == local_credentials_forms_.end()) {
    local_credentials_forms_.push_back(
        std::make_unique<PasswordForm>(form_manager_->GetPendingCredentials()));
  }

  origin_ = url::Origin::Create(form_manager_->GetURL());
  SetState(state);
}

void ManagePasswordsState::OnPasswordAutofilled(
    base::span<const PasswordForm> password_forms,
    url::Origin origin,
    base::span<const PasswordForm> federated_matches) {
  CHECK(!password_forms.empty() || !federated_matches.empty());
  auto local_credentials_forms = DeepCopyNonPSLVector(password_forms);
  AppendDeepCopyVector(federated_matches, &local_credentials_forms);

  // Delete |form_manager_| only when the parameters are processed. They may be
  // coming from |form_manager_|.
  ClearData();

  if (local_credentials_forms.empty()) {
    // Don't show the UI for PSL matched passwords. They are not stored for this
    // page and cannot be deleted.
    OnInactive();
  } else {
    origin_ = std::move(origin);
    local_credentials_forms_ = std::move(local_credentials_forms);
    SetState(password_manager::ui::MANAGE_STATE);
  }
}

void ManagePasswordsState::OnInactive() {
  ClearData();
  origin_ = url::Origin();
  SetState(password_manager::ui::INACTIVE_STATE);
}

void ManagePasswordsState::OnPasswordMovable(
    std::unique_ptr<PasswordFormManagerForUI> form_to_move) {
  ClearData();
  form_manager_ = std::move(form_to_move);
  local_credentials_forms_ =
      DeepCopyNonPSLVector(form_manager_->GetBestMatches());
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);
  origin_ = url::Origin::Create(form_manager_->GetURL());
  SetState(password_manager::ui::MOVE_CREDENTIAL_AFTER_LOG_IN_STATE);
}

void ManagePasswordsState::OnKeychainError() {
  ClearData();
  SetState(password_manager::ui::KEYCHAIN_ERROR_STATE);
}

void ManagePasswordsState::OnPasskeySaved(bool gpm_pin_created) {
  ClearData();
  gpm_pin_created_during_recent_passkey_creation_ = gpm_pin_created;
  SetState(password_manager::ui::PASSKEY_SAVED_CONFIRMATION_STATE);
}

void ManagePasswordsState::OnPasskeyDeleted() {
  ClearData();
  SetState(password_manager::ui::PASSKEY_DELETED_CONFIRMATION_STATE);
}

void ManagePasswordsState::OnPasskeyUpdated() {
  ClearData();
  SetState(password_manager::ui::PASSKEY_UPDATED_CONFIRMATION_STATE);
}

void ManagePasswordsState::OnPasskeyNotAccepted() {
  ClearData();
  SetState(password_manager::ui::PASSKEY_NOT_ACCEPTED_STATE);
}

void ManagePasswordsState::TransitionToState(
    password_manager::ui::State state) {
  CHECK_NE(password_manager::ui::INACTIVE_STATE, state_);
  CHECK(state == password_manager::ui::MANAGE_STATE ||
        state == password_manager::ui::PENDING_PASSWORD_STATE ||
        state == password_manager::ui::PASSWORD_UPDATED_SAFE_STATE ||
        state == password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX ||
        state ==
            password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE ||
        state ==
            password_manager::ui::BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE ||
        state == password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS ||
        state ==
            password_manager::ui::MOVE_CREDENTIAL_FROM_MANAGE_BUBBLE_STATE ||
        state == password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE)
      << state_;
  if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    if (!credentials_callback_.is_null()) {
      std::move(credentials_callback_).Run(nullptr);
    }
  }
  SetState(state);
}

void ManagePasswordsState::ProcessLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  if (state() == password_manager::ui::INACTIVE_STATE) {
    return;
  }

  bool applied_delete = false;
  bool all_changes_are_deletion = true;
  for (const password_manager::PasswordStoreChange& change : changes) {
    if (change.type() != password_manager::PasswordStoreChange::REMOVE) {
      all_changes_are_deletion = false;
    }
    const PasswordForm& changed_form = change.form();
    if (changed_form.blocked_by_user) {
      continue;
    }
    if (change.type() == password_manager::PasswordStoreChange::REMOVE) {
      if (RemoveFormFromVector(changed_form, &local_credentials_forms_)) {
        applied_delete = true;
      }
    } else if (change.type() == password_manager::PasswordStoreChange::UPDATE) {
      UpdateFormInVector(changed_form, &local_credentials_forms_);
    } else {
      DCHECK_EQ(password_manager::PasswordStoreChange::ADD, change.type());
      AddForm(changed_form);
    }
  }
  // Let the password manager know that it should update the list of the
  // credentials. We react only to deletion because in case the password manager
  // itself adds a credential, they should not be refetched. The password
  // generation can be confused as the generated password will be refetched and
  // autofilled immediately.
  if (applied_delete && all_changes_are_deletion) {
    client_->UpdateFormManagers();
  }
}

void ManagePasswordsState::ProcessUnsyncedCredentialsWillBeDeleted(
    std::vector<password_manager::PasswordForm> unsynced_credentials) {
  unsynced_credentials_ = std::move(unsynced_credentials);
  SetState(password_manager::ui::WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE);
}

void ManagePasswordsState::ChooseCredential(const PasswordForm* form) {
  DCHECK_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, state());
  DCHECK(!credentials_callback_.is_null());

  std::move(credentials_callback_).Run(form);
}

void ManagePasswordsState::OpenPasswordDetailsBubble(
    const password_manager::PasswordForm& form) {
  single_credential_mode_credential_ = form;
  SetState(password_manager::ui::State::MANAGE_STATE);
}

void ManagePasswordsState::ClearData() {
  form_manager_.reset();
  clear_selected_password();
  local_credentials_forms_.clear();
  credentials_callback_.Reset();
  unsynced_credentials_.clear();
  single_credential_mode_credential_.reset();
}

bool ManagePasswordsState::AddForm(const PasswordForm& form) {
  if (url::Origin::Create(form.url) != origin_) {
    return false;
  }
  if (UpdateFormInVector(form, &local_credentials_forms_)) {
    return true;
  }
  local_credentials_forms_.push_back(std::make_unique<PasswordForm>(form));
  return true;
}

void ManagePasswordsState::SetState(password_manager::ui::State state) {
  DCHECK(client_);
  if (client_->GetLogManager()->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        client_->GetLogManager());
    logger.LogNumber(autofill::SavePasswordProgressLogger::STRING_NEW_UI_STATE,
                     state);
  }
  state_ = state;
}
