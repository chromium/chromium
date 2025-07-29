// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor_login {

using autofill::FormData;
using autofill::FormFieldData;
using autofill::test::CreateTestFormField;
using password_manager::FakeFormFetcher;
using password_manager::MockPasswordFormCache;
using password_manager::MockPasswordManager;
using password_manager::PasswordForm;
using password_manager::PasswordFormCache;
using password_manager::PasswordFormManager;
using password_manager::PasswordManagerInterface;
using password_manager::PasswordSaveManagerImpl;
using password_manager::StubPasswordManagerClient;
using password_manager::StubPasswordManagerDriver;
using testing::_;
using testing::Return;
using testing::ReturnRef;

namespace {

class MockStubPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(const url::Origin&,
              GetLastCommittedOrigin,
              (),
              (const, override));
};

}  // namespace

class ActorLoginCredentialFillerTest : public ::testing::Test {
 public:
  ActorLoginCredentialFillerTest() = default;
  ~ActorLoginCredentialFillerTest() override = default;

  void SetUp() override {
    // Used by `PasswordFormManager`.
    OSCryptMocker::SetUp();

    ON_CALL(mock_password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&mock_form_cache_));
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  std::unique_ptr<PasswordFormManager> CreateFormManagerWithParsedForm(
      const url::Origin& origin,
      const autofill::FormData& form_data) {
    ON_CALL(mock_driver_, GetLastCommittedOrigin())
        .WillByDefault(ReturnRef(origin));
    auto form_manager = std::make_unique<PasswordFormManager>(
        &stub_client_, mock_driver_.AsWeakPtr(), form_data, &form_fetcher_,
        std::make_unique<PasswordSaveManagerImpl>(&stub_client_),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_fetcher_.NotifyFetchCompleted();
    return form_manager;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  MockPasswordManager mock_password_manager_;
  MockPasswordFormCache mock_form_cache_;
  StubPasswordManagerClient stub_client_;
  MockStubPasswordManagerDriver mock_driver_;
  FakeFormFetcher form_fetcher_;
};

TEST_F(ActorLoginCredentialFillerTest, NoSigninForm_NoManagers) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com/login"));
  Credential credential = CreateTestCredential(u"username", origin.GetURL());
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential, future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginCredentialFillerTest, NoSigninForm_DifferentOrigin) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com/login"));
  url::Origin other_origin =
      url::Origin::Create(GURL("https://other.com/login"));
  Credential credential = CreateTestCredential(u"username", origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      CreateFormManagerWithParsedForm(
          other_origin, CreateSigninFormData(other_origin.GetURL()));
  form_managers.push_back(std::move(form_manager));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential, future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginCredentialFillerTest, NoSigninForm_NoParsedForm) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com/login"));
  Credential credential = CreateTestCredential(u"username", origin.GetURL());
  FormData form_data = CreateSigninFormData(origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  EXPECT_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillOnce(ReturnRef(origin));
  std::unique_ptr<PasswordFormManager> form_manager =
      std::make_unique<PasswordFormManager>(
          &stub_client_, mock_driver_.AsWeakPtr(), form_data, &form_fetcher_,
          std::make_unique<PasswordSaveManagerImpl>(&stub_client_),
          /*metrics_recorder=*/nullptr);

  form_managers.push_back(std::move(form_manager));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential, future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginCredentialFillerTest, NoSigninForm_NotLoginForm) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com/login"));
  Credential credential = CreateTestCredential(u"username", origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      CreateFormManagerWithParsedForm(
          origin, CreateChangePasswordFormData(origin.GetURL()));
  form_managers.push_back(std::move(form_manager));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential, future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

}  // namespace actor_login
