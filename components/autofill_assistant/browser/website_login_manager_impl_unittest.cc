// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/website_login_manager_impl.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormFieldData;
using password_manager::PasswordForm;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArg;

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

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(GetProfilePasswordStore,
                     password_manager::PasswordStore*());
  MOCK_CONST_METHOD0(GetAccountPasswordStore,
                     password_manager::PasswordStore*());
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

PasswordForm MakeSimplePasswordForm() {
  PasswordForm form;
  form.url = GURL(kFakeUrl);
  form.signon_realm = form.url.GetOrigin().spec();
  form.password_value = kFakePassword;
  form.username_value = kFakeUsername16;
  form.username_element = kUsernameElement;
  form.password_element = kPasswordElement;
  form.in_store = PasswordForm::Store::kProfileStore;

  return form;
}

PasswordForm MakeSimplePasswordFormWithoutUsername() {
  PasswordForm form;
  form.url = GURL(kFakeUrl);
  form.signon_realm = form.url.GetOrigin().spec();
  form.password_value = kFakeNewPassword16;
  form.in_store = PasswordForm::Store::kProfileStore;

  return form;
}

}  // namespace

class WebsiteLoginManagerImplTest : public testing::Test {
 public:
  WebsiteLoginManagerImplTest() = default;

 protected:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    profile_store_ = new password_manager::MockPasswordStore;
    ASSERT_TRUE(profile_store_->Init(/*prefs=*/nullptr));

    ON_CALL(client_, GetProfilePasswordStore())
        .WillByDefault(Return(profile_store_.get()));

    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      account_store_ = new password_manager::MockPasswordStore;
      ASSERT_TRUE(account_store_->Init(/*prefs=*/nullptr));

      ON_CALL(client_, GetAccountPasswordStore())
          .WillByDefault(Return(account_store_.get()));
    }
    ON_CALL(*store(), GetLogins(_, _))
        .WillByDefault(
            WithArg<1>([](password_manager::PasswordStoreConsumer* consumer) {
              std::vector<std::unique_ptr<PasswordForm>> result;
              result.push_back(
                  std::make_unique<PasswordForm>(MakeSimplePasswordForm()));
              consumer->OnGetPasswordStoreResults(std::move(result));
            }));

    manager_ = std::make_unique<WebsiteLoginManagerImpl>(&client_,
                                                         web_contents_.get());
  }

  void TearDown() override {
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
    }
    profile_store_->ShutdownOnUIThread();
  }

  WebsiteLoginManagerImpl* manager() { return manager_.get(); }
  password_manager::MockPasswordStore* store() { return profile_store_.get(); }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockPasswordManagerClient> client_;
  std::unique_ptr<WebsiteLoginManagerImpl> manager_;
  scoped_refptr<password_manager::MockPasswordStore> profile_store_;
  scoped_refptr<password_manager::MockPasswordStore> account_store_;
};

// Checks if PasswordForm matches other PasswordForm.
MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.url == arg.url &&
         form.username_element == arg.username_element &&
         form.username_value == arg.username_value &&
         form.password_element == arg.password_element &&
         form.password_value == arg.password_value;
}

TEST_F(WebsiteLoginManagerImplTest, SaveGeneratedPassword) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml, kFakeUrl, GURL(kFakeUrl));
  // Presave generated password. Form with empty username is presaved.
  EXPECT_CALL(*store(), GetLogins(form_digest, _));
  EXPECT_CALL(*store(),
              AddLogin(FormMatches(MakeSimplePasswordFormWithoutUsername())));
  manager()->PresaveGeneratedPassword(
      {GURL(kFakeUrl), kFakeUsername}, kFakeNewPassword,
      MakeFormDataWithPasswordField(), base::OnceClosure());

  // Commit generated password.
  EXPECT_TRUE(manager()->ReadyToCommitGeneratedPassword());
  PasswordForm new_form = MakeSimplePasswordForm();
  new_form.password_value = kFakeNewPassword16;
  // Check that additional data is populated correctly from matched form.
  EXPECT_CALL(*store(), UpdateLoginWithPrimaryKey(FormMatches(new_form), _));
  manager()->CommitGeneratedPassword();
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, DeletePasswordSuccess) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml, kFakeUrl, GURL());
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // |DeletePasswordForLogin| will first fetch all existing logins
  EXPECT_CALL(*store(), GetLogins(form_digest, _));
  EXPECT_CALL(*store(), RemoveLogin(FormMatches(MakeSimplePasswordForm())));
  EXPECT_CALL(mock_callback, Run(true)).Times(1);
  manager()->DeletePasswordForLogin({GURL(kFakeUrl), kFakeUsername},
                                    mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, DeletePasswordFailed) {
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // |DeletePasswordForLogin| will first fetch all existing logins
  EXPECT_CALL(*store(), GetLogins);
  EXPECT_CALL(*store(), RemoveLogin).Times(0);
  EXPECT_CALL(mock_callback, Run(false)).Times(1);
  manager()->DeletePasswordForLogin({GURL(kFakeUrl2), kFakeUsername2},
                                    mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, EditPasswordSuccess) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml, kFakeUrl, GURL());
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // |EditPasswordForLogin| will first fetch all existing logins
  EXPECT_CALL(*store(), GetLogins(form_digest, _));
  PasswordForm new_form = MakeSimplePasswordForm();
  new_form.password_value = kFakeNewPassword16;
  // Check that additional data is populated correctly from matched form.
  EXPECT_CALL(*store(), UpdateLogin(FormMatches(new_form)));
  EXPECT_CALL(mock_callback, Run(true)).Times(1);
  manager()->EditPasswordForLogin({GURL(kFakeUrl), kFakeUsername},
                                  kFakeNewPassword, mock_callback.Get());
  WaitForPasswordStore();
}

TEST_F(WebsiteLoginManagerImplTest, EditPasswordFailed) {
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  // |EditPasswordForLogin| will first fetch all existing logins
  EXPECT_CALL(*store(), GetLogins);
  EXPECT_CALL(*store(), UpdateLogin).Times(0);
  EXPECT_CALL(mock_callback, Run(false)).Times(1);
  manager()->EditPasswordForLogin({GURL(kFakeUrl2), kFakeUsername2},
                                  kFakeNewPassword, mock_callback.Get());
  WaitForPasswordStore();
}

}  // namespace autofill_assistant
