// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_username_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

using autofill::FormData;
using autofill::FormFieldData;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

FormData CreateSigninFormData(const GURL& url, const char* username) {
  FormData form;
  form.set_url(url);
  FormFieldData field;
  field.set_name(u"username_element");
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_value(ASCIIToUTF16(username));
  test_api(form).Append(field);

  field.set_name(u"password_element");
  field.set_form_control_type(autofill::FormControlType::kInputPassword);
  field.set_value(u"strong_pw");
  test_api(form).Append(field);
  return form;
}

}  // namespace

SyncUsernameTestBase::SyncUsernameTestBase() {
  // Start TestSyncService signed out by default to be consistent with
  // IdentityManager, until FakeSigninAs() is invoked.
  CHECK(!identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  sync_service_.SetSignedOut();
}

SyncUsernameTestBase::~SyncUsernameTestBase() = default;

void SyncUsernameTestBase::FakeSigninAs(const std::string& email,
                                        signin::ConsentLevel consent_level) {
  CHECK(!email.empty());
  // This method is called in a roll by some tests. IdentityTestEnvironment does
  // not allow logging in without a previously log-out.
  // So make sure tests only log in once and that the email is the same in case
  // of FakeSigninAs calls roll.
  signin::IdentityManager* identity_manager =
      identity_test_env_.identity_manager();
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    CHECK_EQ(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email,
        email);
    CHECK_EQ(*signin::GetPrimaryAccountConsentLevel(identity_manager),
             consent_level);
    CHECK_EQ(sync_service_.GetAccountInfo().email, email);
  } else {
    CoreAccountInfo account =
        identity_test_env_.MakePrimaryAccountAvailable(email, consent_level);
    sync_service_.SetSignedIn(consent_level, account);
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
  form.match_type = PasswordForm::MatchType::kExact;
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
  form.match_type = PasswordForm::MatchType::kExact;
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
  form.match_type = PasswordForm::MatchType::kExact;
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
