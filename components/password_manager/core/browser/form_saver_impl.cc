// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_util.h"
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
    field.id_attribute.clear();
    field.name_attribute.clear();
  }
}

// Do the clean up of |matches| after |pending| was just pushed to the store.
void PostProcessMatches(const PasswordForm& pending,
                        const std::vector<const PasswordForm*>& matches,
                        const base::string16& old_password,
                        PasswordStore* store) {
  DCHECK(!pending.blacklisted_by_user);

  // Update existing matches in the password store.
  for (const auto* match : matches) {
    DCHECK(pending.preferred);
    if (match->IsFederatedCredential() ||
        ArePasswordFormUniqueKeysEqual(pending, *match))
      continue;
    // Delete obsolete empty username credentials.
    const bool same_password = match->password_value == pending.password_value;
    const bool username_was_added =
        match->username_value.empty() && !pending.username_value.empty();
    if (same_password && username_was_added && !match->is_public_suffix_match) {
      store->RemoveLogin(*match);
      continue;
    }
    base::Optional<PasswordForm> form_to_update;
    const bool same_username = match->username_value == pending.username_value;
    if (same_username) {
      // Maybe update the password value.
      const bool form_has_old_password = match->password_value == old_password;
      if (form_has_old_password) {
        form_to_update = *match;
        form_to_update->password_value = pending.password_value;
      }
    } else if (match->preferred && !match->is_public_suffix_match) {
      // No other credential on the same security origin can be preferred but
      // the most recent one.
      form_to_update = *match;
      form_to_update->preferred = false;
    }
    if (form_to_update) {
      SanitizeFormData(&form_to_update->form_data);
      store->UpdateLogin(std::move(*form_to_update));
    }
  }
}

}  // namespace

FormSaverImpl::FormSaverImpl(PasswordStore* store) : store_(store) {
  DCHECK(store);
}

FormSaverImpl::~FormSaverImpl() = default;

PasswordForm FormSaverImpl::PermanentlyBlacklist(
    PasswordStore::FormDigest digest) {
  PasswordForm blacklisted =
      password_manager_util::MakeNormalizedBlacklistedForm(std::move(digest));
  blacklisted.date_created = base::Time::Now();
  store_->AddLogin(blacklisted);
  return blacklisted;
}

void FormSaverImpl::Unblacklist(const PasswordStore::FormDigest& digest) {
  store_->Unblacklist(digest, /*completion=*/base::DoNothing());
}

void FormSaverImpl::Save(PasswordForm pending,
                         const std::vector<const PasswordForm*>& matches,
                         const base::string16& old_password) {
  SanitizeFormData(&pending.form_data);
  store_->AddLogin(pending);
  // Update existing matches in the password store.
  PostProcessMatches(pending, matches, old_password, store_);
}

void FormSaverImpl::Update(
    autofill::PasswordForm pending,
    const std::vector<const autofill::PasswordForm*>& matches,
    const base::string16& old_password) {
  SanitizeFormData(&pending.form_data);
  store_->UpdateLogin(pending);
  // Update existing matches in the password store.
  PostProcessMatches(pending, matches, old_password, store_);
}

void FormSaverImpl::UpdateReplace(
    autofill::PasswordForm pending,
    const std::vector<const autofill::PasswordForm*>& matches,
    const base::string16& old_password,
    const autofill::PasswordForm& old_unique_key) {
  SanitizeFormData(&pending.form_data);
  store_->UpdateLoginWithPrimaryKey(pending, old_unique_key);
  // Update existing matches in the password store.
  PostProcessMatches(pending, matches, old_password, store_);
}

void FormSaverImpl::Remove(const PasswordForm& form) {
  store_->RemoveLogin(form);
}

std::unique_ptr<FormSaver> FormSaverImpl::Clone() {
  return std::make_unique<FormSaverImpl>(store_);
}

}  // namespace password_manager
