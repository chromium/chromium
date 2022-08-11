// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/password_change/website_login_manager_impl.h"

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {
const char kFakeUrl[] = "http://www.example.com/";
const char kFakeUrl2[] = "https://www.example2.com";
const char kFakeUsername[] = "user@example.com";
const char kFakeUsername2[] = "user2@example.com";
const char16_t kFakeUsername16[] = u"user@example.com";
const char16_t kFakePassword[] = u"old_password";
const char kFakeNewPassword[] = "new_password";
const char16_t kFakeNewPassword16[] = u"new_password";
const char16_t kFormDataName[] = u"the-form-name";
const char16_t kPasswordElement[] = u"password-element";
const char16_t kUsernameElement[] = u"username-element";
const double kDateLastUsedEpoch = 123;

using autofill::FormData;
using autofill::FormFieldData;
using password_manager::PasswordForm;
using password_manager::PasswordManager;
using testing::_;
using testing::Eq;
using testing::Mock;
using testing::Return;
using testing::WithArg;

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetAccountPasswordStore,
              (),
              (const, override));
  MOCK_METHOD(PasswordManager*, GetPasswordManager, (), (const, override));
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
  MOCK_METHOD(password_manager::MockWebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegate,
              (),
              (override));
};

FormData MakeFormDataWithPasswordField() {
  FormData form_data;
  form_data.url = GURL(kFakeUrl);
  form_data.action = GURL(kFakeUrl);
  form_data.name = kFormDataName;

  FormFieldData field;
  field.name = kPasswordElement;
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = kFakeNewPassword16;
  field.form_control_type = "password";
  form_data.fields.push_back(field);

  return form_data;
}

FormData MakeFormDataWithUsernameAndPasswordField() {
  FormData form_data;
  form_data.url = GURL(kFakeUrl);
  form_data.action = GURL(kFakeUrl);
  form_data.name = kFormDataName;

  FormFieldData field;
  field.name = kUsernameElement;
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = kFakeUsername16;
  field.form_control_type = "text";
  form_data.fields.push_back(field);

  field.name = kPasswordElement;
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = kFakePassword;
  field.form_control_type = "password";
  form_data.fields.push_back(field);

  return form_data;
}

PasswordForm MakeSimplePasswordForm() {
  PasswordForm form;
  form.url = GURL(kFakeUrl);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.password_value = kFakePassword;
  form.username_value = kFakeUsername16;
  form.username_element = kUsernameElement;
  form.password_element = kPasswordElement;
  form.in_store = PasswordForm::Store::kProfileStore;
  form.date_last_used = base::Time::FromDoubleT(kDateLastUsedEpoch);

  return form;
}

PasswordForm MakeSimplePasswordFormWithoutUsername() {
  PasswordForm form;
  form.url = GURL(kFakeUrl);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.password_value = kFakeNewPassword16;
  form.in_store = PasswordForm::Store::kProfileStore;

  return form;
}

PasswordForm MakeSimplePasswordFormWithFormData() {
  PasswordForm form = MakeSimplePasswordForm();
  form.form_data = MakeFormDataWithPasswordField();
  return form;
}

PasswordForm MakeSimpleLoginFormWithFormData() {
  PasswordForm form = MakeSimplePasswordForm();
  form.form_data = MakeFormDataWithUsernameAndPasswordField();
  return form;
}

}  // namespace

class WebsiteLoginManagerImplTest : public content::RenderViewHostTestHarness {
 public:
  WebsiteLoginManagerImplTest() = default;

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    profile_store_ = new password_manager::MockPasswordStoreInterface;

    ON_CALL(client_, GetProfilePasswordStore())
        .WillByDefault(Return(profile_store_.get()));

    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      account_store_ = new password_manager::MockPasswordStoreInterface;

      ON_CALL(client_, GetAccountPasswordStore())
          .WillByDefault(Return(account_store_.get()));

      // Never return anything from the account store.
      ON_CALL(*account_store_, GetLogins(_, _))
          .WillByDefault(WithArg<1>(
              [account_store = account_store_.get()](
                  base::WeakPtr<password_manager::PasswordStoreConsumer>
                      consumer) {
                consumer->OnGetPasswordStoreResultsFrom(account_store, {});
              }));
    }
    ON_CALL(*store(), GetLogins(_, _))
        .WillByDefault(WithArg<1>(
            [this](base::WeakPtr<password_manager::PasswordStoreConsumer>
                       consumer) {
              std::vector<std::unique_ptr<PasswordForm>> result;
              result.push_back(
                  std::make_unique<PasswordForm>(MakeSimplePasswordForm()));
              consumer->OnGetPasswordStoreResultsFrom(store(),
                                                      std::move(result));
            }));

    manager_ =
        std::make_unique<WebsiteLoginManagerImpl>(&client_, web_contents());
    password_manager_ = std::make_unique<PasswordManager>(&client_);
    ON_CALL(client_, GetPasswordManager())
        .WillByDefault(Return(password_manager_.get()));
    ON_CALL(client_, GetWebAuthnCredentialsDelegate())
        .WillByDefault(Return(&webauthn_credentials_delegate_));
    ON_CALL(webauthn_credentials_delegate_, IsWebAuthnAutofillEnabled)
        .WillByDefault(Return(false));
  }

  password_manager::MockPasswordStoreInterface* store() {
    return profile_store_.get();
  }

  void WaitForPasswordStore() { task_environment()->RunUntilIdle(); }

 protected:
  testing::NiceMock<MockPasswordManagerClient> client_;
  std::unique_ptr<WebsiteLoginManagerImpl> manager_;
  std::unique_ptr<PasswordManager> password_manager_;
  testing::NiceMock<password_manager::MockWebAuthnCredentialsDelegate>
      webauthn_credentials_delegate_;
  password_manager::StubPasswordManagerDriver driver_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> profile_store_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> account_store_;
};

// Checks if `PasswordForm` matches other `PasswordForm`.
MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.url == arg.url &&
         form.username_element == arg.username_element &&
         form.username_value == arg.username_value &&
         form.password_element == arg.password_element &&
         form.password_value == arg.password_value;
}

ACTION_P(InvokeEmptyConsumerWithForms, store) {
  arg0->OnGetPasswordStoreResultsFrom(
      store, std::vector<std::unique_ptr<PasswordForm>>());
}

TEST_F(WebsiteLoginManagerImplTest, SaveGeneratedPassword) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml, kFakeUrl, GURL(kFakeUrl));
  // Presave generated password. Form with empty username is presaved.
  EXPECT_CALL(*store(), GetLogins(form_digest, _));
  EXPECT_CALL(
      *store(),
      AddLogin(FormMatches(MakeSimplePasswordFormWithoutUsername()), _));
  manager_->PresaveGeneratedPassword(
      {GURL(kFakeUrl), kFakeUsername}, kFakeNewPassword,
      MakeFormDataWithPasswordField(), base::OnceClosure());
  WaitForPasswordStore();

  // Save generated password.
  EXPECT_TRUE(manager_->ReadyToSaveGeneratedPassword());
  PasswordForm new_form = MakeSimplePasswordForm();
  new_form.password_value = kFakeNewPassword16;
  // Check that additional data is populated correctly from matched form.
  EXPECT_CALL(*store(), UpdateLoginWithPrimaryKey(FormMatches(new_form), _));
  manager_->SaveGeneratedPassword();
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, DeletePasswordSuccess) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml, kFakeUrl, GURL());
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // `DeletePasswordForLogin` will first fetch all existing logins.
  EXPECT_CALL(*store(), GetLogins(form_digest, _));
  EXPECT_CALL(*store(), RemoveLogin(FormMatches(MakeSimplePasswordForm())));
  EXPECT_CALL(mock_callback, Run(true)).Times(1);
  manager_->DeletePasswordForLogin({GURL(kFakeUrl), kFakeUsername},
                                   mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, DeletePasswordFailed) {
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // `DeletePasswordForLogin` will first fetch all existing logins.
  EXPECT_CALL(*store(), GetLogins);
  EXPECT_CALL(*store(), RemoveLogin).Times(0);
  EXPECT_CALL(mock_callback, Run(false)).Times(1);
  manager_->DeletePasswordForLogin({GURL(kFakeUrl2), kFakeUsername2},
                                   mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, GetGetLastTimePasswordUsedSuccess) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml, kFakeUrl, GURL());
  base::MockCallback<base::OnceCallback<void(absl::optional<base::Time>)>>
      mock_callback;
  // `GetGetLastTimePasswordUsedSuccess` will first fetch all existing
  // logins.
  EXPECT_CALL(*store(), GetLogins(form_digest, _));
  EXPECT_CALL(mock_callback, Run({base::Time::FromDoubleT(123)})).Times(1);
  manager_->GetGetLastTimePasswordUsed({GURL(kFakeUrl), kFakeUsername},
                                       mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, GetGetLastTimePasswordUsedFailed) {
  base::MockCallback<base::OnceCallback<void(absl::optional<base::Time>)>>
      mock_callback;
  // `GetGetLastTimePasswordUsedSuccess` will first fetch all existing
  // logins.
  EXPECT_CALL(*store(), GetLogins);
  EXPECT_CALL(mock_callback, Run(Eq(absl::nullopt))).Times(1);
  manager_->GetGetLastTimePasswordUsed({GURL(kFakeUrl2), kFakeUsername2},
                                       mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, EditPasswordSuccess) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml, kFakeUrl, GURL());
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // |EditPasswordForLogin| will first fetch all existing logins.
  EXPECT_CALL(*store(), GetLogins(form_digest, _));
  PasswordForm new_form = MakeSimplePasswordForm();
  new_form.password_value = kFakeNewPassword16;
  // Check that additional data is populated correctly from matched form.
  EXPECT_CALL(*store(), UpdateLogin(FormMatches(new_form)));
  EXPECT_CALL(mock_callback, Run(true)).Times(1);
  manager_->EditPasswordForLogin({GURL(kFakeUrl), kFakeUsername},
                                 kFakeNewPassword, mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, EditPasswordFailed) {
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // `EditPasswordForLogin` will first fetch all existing logins.
  EXPECT_CALL(*store(), GetLogins);
  EXPECT_CALL(*store(), UpdateLogin).Times(0);
  EXPECT_CALL(mock_callback, Run(false)).Times(1);
  manager_->EditPasswordForLogin({GURL(kFakeUrl2), kFakeUsername2},
                                 kFakeNewPassword, mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, ResetPendingCredentials) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));

  PasswordForm form = MakeSimplePasswordFormWithFormData();
  password_manager_->OnPasswordFormsParsed(&driver_, {form.form_data});
  EXPECT_FALSE(password_manager_->HasSubmittedManager());

  password_manager_->OnInformAboutUserInput(&driver_, form.form_data);
  password_manager_->OnPasswordFormSubmitted(&driver_, form.form_data);
  EXPECT_TRUE(password_manager_->HasSubmittedManager());
  EXPECT_FALSE(password_manager_->HasSubmittedManagerWithSamePassword());
  EXPECT_TRUE(password_manager_->GetSubmittedManagerForTest());

  manager_->ResetPendingCredentials();
  EXPECT_FALSE(password_manager_->HasSubmittedManager());
  EXPECT_FALSE(password_manager_->HasSubmittedManagerWithSamePassword());
  EXPECT_FALSE(password_manager_->GetSubmittedManagerForTest());
}

TEST_F(WebsiteLoginManagerImplTest, SaveSubmittedPasswordUpdate) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));

  PasswordForm form(MakeSimplePasswordFormWithFormData());
  password_manager_->OnPasswordFormsParsed(&driver_, {form.form_data});

  // The user updates the password of a stored credential.
  FormData updated_data(form.form_data);
  updated_data.fields[0].value = u"updated_password";
  password_manager_->OnInformAboutUserInput(&driver_, updated_data);
  password_manager_->OnPasswordFormSubmitted(&driver_, updated_data);
  EXPECT_TRUE(password_manager_->GetSubmittedManagerForTest());
  EXPECT_TRUE(manager_->ReadyToSaveSubmittedPassword());
  EXPECT_FALSE(manager_->SubmittedPasswordIsSame());

  PasswordForm expected_form(form);
  // The expected form with a new password.
  expected_form.password_value = updated_data.fields[0].value;
  EXPECT_CALL(*store(), UpdateLogin(FormMatches(expected_form)));
  EXPECT_TRUE(manager_->SaveSubmittedPassword());
}

TEST_F(WebsiteLoginManagerImplTest, SaveSubmittedPasswordEqualPassword) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));

  PasswordForm form(MakeSimplePasswordFormWithFormData());
  password_manager_->OnPasswordFormsParsed(&driver_, {form.form_data});

  // The user submits an existing credential.
  FormData non_updated_data(form.form_data);
  non_updated_data.fields[0].value = kFakePassword;
  password_manager_->OnInformAboutUserInput(&driver_, non_updated_data);
  password_manager_->OnPasswordFormSubmitted(&driver_, non_updated_data);
  EXPECT_TRUE(password_manager_->GetSubmittedManagerForTest());
  EXPECT_TRUE(manager_->ReadyToSaveSubmittedPassword());
  EXPECT_TRUE(manager_->SubmittedPasswordIsSame());

  // The expected form with the same password.
  PasswordForm expected_form(form);
  expected_form.password_value = non_updated_data.fields[0].value;
  EXPECT_CALL(*store(), UpdateLogin(FormMatches(expected_form)));
  EXPECT_TRUE(manager_->SaveSubmittedPassword());
}

TEST_F(WebsiteLoginManagerImplTest, SaveSubmittedPasswordNewLogin) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm form(MakeSimpleLoginFormWithFormData());

  EXPECT_CALL(*store(), GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms(store())));

  // The user submits the an entirely new credential.
  password_manager_->OnPasswordFormsParsed(&driver_, {form.form_data});
  password_manager_->OnPasswordFormsRendered(&driver_, {form.form_data});
  password_manager_->OnPasswordFormSubmitted(&driver_, form.form_data);
  EXPECT_TRUE(password_manager_->GetSubmittedManagerForTest());
  EXPECT_TRUE(manager_->ReadyToSaveSubmittedPassword());
  EXPECT_FALSE(manager_->SubmittedPasswordIsSame());

  // Expect the password to get saved.
  EXPECT_CALL(*store(), AddLogin(FormMatches(form), _));
  EXPECT_TRUE(manager_->SaveSubmittedPassword());
}

TEST_F(WebsiteLoginManagerImplTest, SaveSubmittedPasswordFailure) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  password_manager_->OnPasswordFormsParsed(&driver_,
                                           {MakeFormDataWithPasswordField()});
  // No user updates to this point.
  EXPECT_FALSE(password_manager_->GetSubmittedManagerForTest());
  EXPECT_FALSE(manager_->ReadyToSaveSubmittedPassword());
  EXPECT_FALSE(manager_->SubmittedPasswordIsSame());
  EXPECT_FALSE(manager_->SaveSubmittedPassword());
}

TEST_F(WebsiteLoginManagerImplTest, GeneratePasswordForNullRenderFrameHost) {
  EXPECT_EQ(
      absl::nullopt,
      // The arguments other than the `render_frame_host` are arbitrary.
      manager_->GeneratePassword(/*rfh=*/nullptr, autofill::FormSignature(123),
                                 autofill::FieldSignature(456), 10));
}

}  // namespace autofill_assistant
