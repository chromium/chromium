// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_state.h"

#include <algorithm>
#include <utility>

#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"

using autofill::PasswordForm;
using password_manager::PasswordFormManagerForUI;

namespace {

std::vector<std::unique_ptr<PasswordForm>> DeepCopyNonPSLVector(
    const std::vector<const PasswordForm*>& password_forms) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.reserve(password_forms.size());
  for (const PasswordForm* form : password_forms) {
    if (!form->is_public_suffix_match)
      result.push_back(std::make_unique<PasswordForm>(*form));
  }
  return result;
}

void AppendDeepCopyVector(const std::vector<const PasswordForm*>& forms,
                          std::vector<std::unique_ptr<PasswordForm>>* result) {
  result->reserve(result->size() + forms.size());
  for (auto* form : forms)
    result->push_back(std::make_unique<PasswordForm>(*form));
}

// Updates one form in |forms| that has the same unique key as |updated_form|.
// Returns true if the form was found and updated.
bool UpdateFormInVector(const PasswordForm& updated_form,
                        std::vector<std::unique_ptr<PasswordForm>>* forms) {
  auto it =
      std::find_if(forms->begin(), forms->end(),
                   [&updated_form](const std::unique_ptr<PasswordForm>& form) {
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
  auto it = std::find_if(
      forms->begin(), forms->end(),
      [&form_to_delete](const std::unique_ptr<PasswordForm>& form) {
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
    : state_(password_manager::ui::INACTIVE_STATE),
      client_(nullptr) {
}

ManagePasswordsState::~ManagePasswordsState() {}

void ManagePasswordsState::OnPendingPassword(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  local_credentials_forms_ =
      DeepCopyNonPSLVector(form_manager_->GetBestMatches());
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);
  origin_ = form_manager_->GetOrigin();
  SetState(password_manager::ui::PENDING_PASSWORD_STATE);
}

void ManagePasswordsState::OnUpdatePassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  local_credentials_forms_ =
      DeepCopyNonPSLVector(form_manager_->GetBestMatches());
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);
  origin_ = form_manager_->GetOrigin();
  SetState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

void ManagePasswordsState::OnRequestCredentials(
    std::vector<std::unique_ptr<PasswordForm>> local_credentials,
    const GURL& origin) {
  ClearData();
  local_credentials_forms_ = std::move(local_credentials);
  origin_ = origin;
  SetState(password_manager::ui::CREDENTIAL_REQUEST_STATE);
}

void ManagePasswordsState::OnAutoSignin(
    std::vector<std::unique_ptr<PasswordForm>> local_forms,
    const GURL& origin) {
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
  for (const auto* form : form_manager_->GetBestMatches()) {
    if (form->is_public_suffix_match)
      continue;
    local_credentials_forms_.push_back(std::make_unique<PasswordForm>(*form));
  }
  AppendDeepCopyVector(form_manager_->GetFederatedMatches(),
                       &local_credentials_forms_);
  origin_ = form_manager_->GetOrigin();
  SetState(password_manager::ui::CONFIRMATION_STATE);
}

void ManagePasswordsState::OnPasswordAutofilled(
    const std::vector<const PasswordForm*>& password_forms,
    GURL origin,
    const std::vector<const PasswordForm*>* federated_matches) {
  DCHECK(!password_forms.empty() ||
         (federated_matches && !federated_matches->empty()));
  auto local_credentials_forms = DeepCopyNonPSLVector(password_forms);
  if (federated_matches)
    AppendDeepCopyVector(*federated_matches, &local_credentials_forms);

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
  origin_ = GURL();
  SetState(password_manager::ui::INACTIVE_STATE);
}

void ManagePasswordsState::TransitionToState(
    password_manager::ui::State state) {
  DCHECK_NE(password_manager::ui::INACTIVE_STATE, state_);
  DCHECK_EQ(password_manager::ui::MANAGE_STATE, state);
  if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    if (!credentials_callback_.is_null()) {
      credentials_callback_.Run(nullptr);
      credentials_callback_.Reset();
    }
  }
  SetState(state);
}

void ManagePasswordsState::ProcessLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  if (state() == password_manager::ui::INACTIVE_STATE)
    return;

  bool applied_delete = false;
  bool all_changes_are_deletion = true;
  for (const password_manager::PasswordStoreChange& change : changes) {
    if (change.type() != password_manager::PasswordStoreChange::REMOVE)
      all_changes_are_deletion = false;
    const PasswordForm& changed_form = change.form();
    if (changed_form.blacklisted_by_user)
      continue;
    if (change.type() == password_manager::PasswordStoreChange::REMOVE) {
      if (RemoveFormFromVector(changed_form, &local_credentials_forms_))
        applied_delete = true;
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
  if (applied_delete && all_changes_are_deletion)
    client_->UpdateFormManagers();
}

void ManagePasswordsState::ChooseCredential(const PasswordForm* form) {
  DCHECK_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, state());
  DCHECK(!credentials_callback().is_null());

  credentials_callback().Run(form);
  set_credentials_callback(ManagePasswordsState::CredentialsCallback());
}

void ManagePasswordsState::ClearData() {
  form_manager_.reset();
  local_credentials_forms_.clear();
  credentials_callback_.Reset();
}

bool ManagePasswordsState::AddForm(const PasswordForm& form) {
  if (form.origin.GetOrigin() != origin_.GetOrigin())
    return false;
  if (UpdateFormInVector(form, &local_credentials_forms_))
    return true;
  local_credentials_forms_.push_back(std::make_unique<PasswordForm>(form));
  return true;
}

void ManagePasswordsState::SetState(password_manager::ui::State state) {
  DCHECK(client_);
  if (client_->GetLogManager()->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        client_->GetLogManager());
    logger.LogNumber(
        autofill::SavePasswordProgressLogger::STRING_NEW_UI_STATE,
        state);
  }
  state_ = state;
}
