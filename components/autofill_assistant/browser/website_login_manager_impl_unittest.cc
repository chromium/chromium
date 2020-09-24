// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/website_login_manager_impl.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArg;

namespace autofill_assistant {

namespace {
const char kFakeUrl[] = "http://www.example.com/";
const char kFakeUsername[] = "user@example.com";
const char kFakePassword[] = "old_password";
const char kFakeNewPassword[] = "new_password";
const char kFormDataName[] = "the-form-name";
const char kPasswordElement[] = "password-element";
const char kUsernameElement[] = "username-element";

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
  form_data.name = ASCIIToUTF16(kFormDataName);

  FormFieldData field;
  field.name = ASCIIToUTF16(kPasswordElement);
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = ASCIIToUTF16(kFakeNewPassword);
  field.form_control_type = "password";
  form_data.fields.push_back(field);

  return form_data;
}

PasswordForm MakeSimplePasswordForm() {
  PasswordForm form;
  form.url = GURL(kFakeUrl);
  form.signon_realm = form.url.GetOrigin().spec();
  form.password_value = ASCIIToUTF16(kFakePassword);
  form.username_value = ASCIIToUTF16(kFakeUsername);
  form.username_element = ASCIIToUTF16(kUsernameElement);
  form.password_element = ASCIIToUTF16(kPasswordElement);
  form.in_store = PasswordForm::Store::kProfileStore;

  return form;
}

PasswordForm MakeSimplePasswordFormWithoutUsername() {
  PasswordForm form;
  form.url = GURL(kFakeUrl);
  form.signon_realm = form.url.GetOrigin().spec();
  form.password_value = ASCIIToUTF16(kFakeNewPassword);
  form.in_store = PasswordForm::Store::kProfileStore;

  return form;
}

}  // namespace

class WebsiteLoginManagerImplTest : public content::RenderViewHostTestHarness {
 public:
  WebsiteLoginManagerImplTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~WebsiteLoginManagerImplTest() override = default;

 protected:
  void SetUp() override {
    profile_store_ = new password_manager::MockPasswordStore;
    ON_CALL(*profile_store_, IsAccountStore()).WillByDefault(Return(false));
    ASSERT_TRUE(profile_store_->Init(/*prefs=*/nullptr));

    ON_CALL(client_, GetProfilePasswordStore())
        .WillByDefault(Return(profile_store_.get()));

    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      account_store_ = new password_manager::MockPasswordStore;
      ON_CALL(*account_store_, IsAccountStore()).WillByDefault(Return(true));
      ASSERT_TRUE(account_store_->Init(/*prefs=*/nullptr));

      ON_CALL(client_, GetAccountPasswordStore())
          .WillByDefault(Return(account_store_.get()));
    }

    manager_ =
        std::make_unique<WebsiteLoginManagerImpl>(&client_, web_contents());
  }

  void TearDown() override {
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
    }
    profile_store_->ShutdownOnUIThread();
  }

  WebsiteLoginManagerImpl* manager() { return manager_.get(); }
  password_manager::MockPasswordStore* store() { return profile_store_.get(); }

  void WaitForPasswordStore() { task_environment()->RunUntilIdle(); }

 private:
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
  ON_CALL(*store(), GetLogins(_, _))
      .WillByDefault(WithArg<1>(
          Invoke([](password_manager::PasswordStoreConsumer* consumer) {
            std::vector<std::unique_ptr<PasswordForm>> result;
            result.push_back(
                std::make_unique<PasswordForm>(MakeSimplePasswordForm()));
            consumer->OnGetPasswordStoreResults(std::move(result));
          })));

  password_manager::PasswordStore::FormDigest form_digest(
      autofill::PasswordForm::Scheme::kHtml, kFakeUrl, GURL(kFakeUrl));
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
  new_form.password_value = ASCIIToUTF16(kFakeNewPassword);
  // Check that additional data is populated correctly from matched form.
  EXPECT_CALL(*store(), UpdateLoginWithPrimaryKey(FormMatches(new_form), _));
  manager()->CommitGeneratedPassword();
  WaitForPasswordStore();
}

}  // namespace autofill_assistant
