// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;
using testing::Invoke;

namespace password_manager {

namespace {

class MockDelegate : public CredentialManagerPasswordFormManagerDelegate {
 public:
  MOCK_METHOD0(OnProvisionalSaveComplete, void());
};

class MockFormSaver : public StubFormSaver {
 public:
  MockFormSaver() = default;
  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD3(Save,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));
  MOCK_METHOD3(Update,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));

  // Convenience downcasting method.
  static MockFormSaver& Get(PasswordFormManager* form_manager) {
    return *static_cast<MockFormSaver*>(form_manager->form_saver());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFormSaver);
};

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.origin == arg.origin &&
         form.username_value == arg.username_value &&
         form.password_value == arg.password_value &&
         form.scheme == arg.scheme && form.type == arg.type;
}

}  // namespace

class CredentialManagerPasswordFormManagerTest : public testing::Test {
 public:
  CredentialManagerPasswordFormManagerTest() {
    form_to_save_.origin = GURL("https://example.com/path");
    form_to_save_.signon_realm = "https://example.com/";
    form_to_save_.username_value = ASCIIToUTF16("user1");
    form_to_save_.password_value = ASCIIToUTF16("pass1");
    form_to_save_.scheme = PasswordForm::Scheme::kHtml;
    form_to_save_.type = PasswordForm::Type::kApi;
  }

 protected:
  std::unique_ptr<CredentialManagerPasswordFormManager> CreateFormManager(
      const PasswordForm& form_to_save) {
    std::unique_ptr<FakeFormFetcher> fetcher(new FakeFormFetcher());
    std::unique_ptr<MockFormSaver> saver(new MockFormSaver());
    return std::make_unique<CredentialManagerPasswordFormManager>(
        &client_, std::make_unique<PasswordForm>(form_to_save), &delegate_,
        std::make_unique<MockFormSaver>(), std::make_unique<FakeFormFetcher>());
  }

  void SetNonFederatedAndNotifyFetchCompleted(
      FormFetcher* fetcher,
      const std::vector<const PasswordForm*>& non_federated) {
    auto* fake_fetcher = static_cast<FakeFormFetcher*>(fetcher);
    fake_fetcher->SetNonFederated(non_federated);
    fake_fetcher->NotifyFetchCompleted();
    // It is required because of PostTask in
    // CredentialManagerPasswordFormManager::OnFetchCompleted
    base::RunLoop().RunUntilIdle();
  }

  // Necessary for callbacks, and for TestAutofillDriver.
  base::test::SingleThreadTaskEnvironment task_environment_;

  StubPasswordManagerClient client_;
  MockDelegate delegate_;
  PasswordForm form_to_save_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerPasswordFormManagerTest);
};

// Ensure that GetCredentialSource is actually overriden and returns the proper
// value.
TEST_F(CredentialManagerPasswordFormManagerTest, GetCredentialSource) {
  MockDelegate delegate;
  auto form_manager = std::make_unique<CredentialManagerPasswordFormManager>(
      &client_, std::make_unique<PasswordForm>(), &delegate,
      std::make_unique<StubFormSaver>(), std::make_unique<FakeFormFetcher>());
  ASSERT_EQ(metrics_util::CredentialSourceType::kCredentialManagementAPI,
            form_manager->GetCredentialSource());
}

TEST_F(CredentialManagerPasswordFormManagerTest, SaveCredentialAPIEmptyStore) {
  std::unique_ptr<CredentialManagerPasswordFormManager> form_manager =
      CreateFormManager(form_to_save_);
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager.get());
  EXPECT_CALL(delegate_, OnProvisionalSaveComplete());
  SetNonFederatedAndNotifyFetchCompleted(form_manager->GetFormFetcher(), {});
  EXPECT_TRUE(form_manager->IsNewLogin());
  EXPECT_TRUE(form_manager->is_submitted());

  EXPECT_CALL(form_saver, Save(FormMatches(form_to_save_), _, _));
  form_manager->Save();
}

TEST_F(CredentialManagerPasswordFormManagerTest,
       SaveCredentialAPINonEmptyStore) {
  // Simulate that the password store has crendentials with different
  // username/password as a submitted one.
  PasswordForm saved_match = form_to_save_;
  saved_match.username_value += ASCIIToUTF16("1");
  saved_match.password_value += ASCIIToUTF16("1");

  std::unique_ptr<CredentialManagerPasswordFormManager> form_manager =
      CreateFormManager(form_to_save_);
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager.get());

  EXPECT_CALL(delegate_, OnProvisionalSaveComplete());
  SetNonFederatedAndNotifyFetchCompleted(form_manager->GetFormFetcher(),
                                         {&saved_match});
  EXPECT_TRUE(form_manager->IsNewLogin());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_EQ(form_to_save_.origin, form_manager->GetOrigin());

  EXPECT_CALL(form_saver, Save(FormMatches(form_to_save_), _, _));
  form_manager->Save();
}

TEST_F(CredentialManagerPasswordFormManagerTest, UpdatePasswordCredentialAPI) {
  // Simulate that the submitted credential has the same username but the
  // different password from already saved one.
  PasswordForm saved_match = form_to_save_;
  saved_match.password_value += ASCIIToUTF16("1");

  std::unique_ptr<CredentialManagerPasswordFormManager> form_manager =
      CreateFormManager(form_to_save_);
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager.get());

  EXPECT_CALL(delegate_, OnProvisionalSaveComplete());
  SetNonFederatedAndNotifyFetchCompleted(form_manager->GetFormFetcher(),
                                         {&saved_match});
  EXPECT_FALSE(form_manager->IsNewLogin());
  EXPECT_TRUE(form_manager->is_submitted());

  EXPECT_CALL(form_saver, Update(FormMatches(form_to_save_), _, _));
  form_manager->Save();
}

}  // namespace password_manager
