// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_username_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/password_form.h"

using autofill::FormData;
using autofill::FormFieldData;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

FormData CreateSigninFormData(const GURL& url, const char* username) {
  FormData form;
  form.url = url;
  FormFieldData field;
  field.name = u"username_element";
  field.form_control_type = "text";
  field.value = ASCIIToUTF16(username);
  form.fields.push_back(field);

  field.name = u"password_element";
  field.form_control_type = "password";
  field.value = u"strong_pw";
  form.fields.push_back(field);
  return form;
}

}  // namespace

SyncUsernameTestBase::SyncUsernameTestBase() = default;

SyncUsernameTestBase::~SyncUsernameTestBase() = default;

void SyncUsernameTestBase::FakeSigninAs(const std::string& email) {
  // This method is called in a roll by some tests. IdentityTestEnvironment does
  // not allow logging in without a previously log-out.
  // So make sure tests only log in once and that the email is the same in case
  // of FakeSigninAs calls roll.
  signin::IdentityManager* identity_manager =
      identity_test_env_.identity_manager();
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    DCHECK_EQ(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
            .email,
        email);
  } else {
    identity_test_env_.MakePrimaryAccountAvailable(email,
                                                   signin::ConsentLevel::kSync);
  }
}

// static
PasswordForm SyncUsernameTestBase::SimpleGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://accounts.google.com";
  form.url = GURL("https://accounts.google.com");
  form.username_value = ASCIIToUTF16(username);
  form.form_data = CreateSigninFormData(GURL(form.signon_realm), username);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.url = GURL("https://site.com");
  form.username_value = ASCIIToUTF16(username);
  form.form_data = CreateSigninFormData(GURL(form.signon_realm), username);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username,
                                                     const char* origin) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.username_value = ASCIIToUTF16(username);
  form.url = GURL(origin);
  form.form_data = CreateSigninFormData(GURL(form.signon_realm), username);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

void SyncUsernameTestBase::SetSyncingPasswords(bool syncing_passwords) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncing_passwords
          ? syncer::UserSelectableTypeSet(
                {syncer::UserSelectableType::kPasswords})
          : syncer::UserSelectableTypeSet());
}

}  // namespace password_manager
