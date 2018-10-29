// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;

namespace password_manager {

namespace {

// Remove all information from |form| that is not required for signature
// calculation.
void SanitizeFormData(FormData* form) {
  form->main_frame_origin = url::Origin();
  for (FormFieldData& field : form->fields) {
    field.label.clear();
    field.value.clear();
    field.autocomplete_attribute.clear();
    field.option_values.clear();
    field.option_contents.clear();
    field.placeholder.clear();
    field.css_classes.clear();
    field.id.clear();
  }
}

}  // namespace

FormSaverImpl::FormSaverImpl(PasswordStore* store) : store_(store) {
  DCHECK(store);
}

FormSaverImpl::~FormSaverImpl() = default;

void FormSaverImpl::PermanentlyBlacklist(PasswordForm* observed) {
  observed->preferred = false;
  observed->blacklisted_by_user = true;
  observed->username_value.clear();
  observed->password_value.clear();
  observed->other_possible_usernames.clear();
  observed->date_created = base::Time::Now();

  store_->AddLogin(*observed);
}

void FormSaverImpl::Save(
    const PasswordForm& pending,
    const std::map<base::string16, const PasswordForm*>& best_matches) {
  SaveImpl(pending, true, best_matches, nullptr, nullptr);
}

void FormSaverImpl::Update(
    const PasswordForm& pending,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<PasswordForm>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  SaveImpl(pending, false, best_matches, credentials_to_update,
           old_primary_key);
}

void FormSaverImpl::PresaveGeneratedPassword(const PasswordForm& generated) {
  auto form = std::make_unique<PasswordForm>(generated);
  SanitizeFormData(&form->form_data);
  if (presaved_)
    store_->UpdateLoginWithPrimaryKey(*form, *presaved_);
  else
    store_->AddLogin(*form);
  presaved_ = std::move(form);
}

void FormSaverImpl::RemovePresavedPassword() {
  if (!presaved_)
    return;

  store_->RemoveLogin(*presaved_);
  presaved_ = nullptr;
}

std::unique_ptr<FormSaver> FormSaverImpl::Clone() {
  auto result = std::make_unique<FormSaverImpl>(store_);
  if (presaved_)
    result->presaved_ = std::make_unique<PasswordForm>(*presaved_);
  return result;
}

void FormSaverImpl::SaveImpl(
    const PasswordForm& pending,
    bool is_new_login,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<PasswordForm>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  DCHECK(pending.preferred);
  DCHECK(!pending.blacklisted_by_user);

  UpdatePreferredLoginState(pending.username_value, best_matches);
  PasswordForm sanitized_pending(pending);
  SanitizeFormData(&sanitized_pending.form_data);
  if (presaved_) {
    store_->UpdateLoginWithPrimaryKey(sanitized_pending, *presaved_);
    presaved_ = nullptr;
  } else if (is_new_login) {
    store_->AddLogin(sanitized_pending);
    if (!sanitized_pending.username_value.empty())
      DeleteEmptyUsernameCredentials(sanitized_pending, best_matches);
  } else {
    if (old_primary_key)
      store_->UpdateLoginWithPrimaryKey(sanitized_pending, *old_primary_key);
    else
      store_->UpdateLogin(sanitized_pending);
  }

  if (credentials_to_update) {
    for (const PasswordForm& credential : *credentials_to_update) {
      store_->UpdateLogin(credential);
    }
  }
}

void FormSaverImpl::UpdatePreferredLoginState(
    const base::string16& preferred_username,
    const std::map<base::string16, const PasswordForm*>& best_matches) {
  for (const auto& key_value_pair : best_matches) {
    const PasswordForm& form = *key_value_pair.second;
    if (form.preferred && !form.is_public_suffix_match &&
        form.username_value != preferred_username) {
      // This wasn't the selected login but it used to be preferred.
      PasswordForm update(form);
      SanitizeFormData(&update.form_data);
      update.preferred = false;
      store_->UpdateLogin(update);
    }
  }
}

void FormSaverImpl::DeleteEmptyUsernameCredentials(
    const PasswordForm& pending,
    const std::map<base::string16, const PasswordForm*>& best_matches) {
  DCHECK(!pending.username_value.empty());

  for (const auto& match : best_matches) {
    const PasswordForm* form = match.second;
    if (!form->is_public_suffix_match && form->username_value.empty() &&
        form->password_value == pending.password_value) {
      store_->RemoveLogin(*form);
    }
  }
}

}  // namespace password_manager
