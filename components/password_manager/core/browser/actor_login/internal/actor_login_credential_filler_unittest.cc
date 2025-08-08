// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
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
using testing::Eq;
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

password_manager::PasswordForm CreateSavedPasswordForm(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password = u"") {
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = password_manager::GetSignonRealm(url);
  form.username_value = username;
  form.password_value = password;
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

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
    form_manager->DisableFillingServerPredictionsForTesting();
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

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential, mock_callback.Get());

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_);
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

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential, mock_callback.Get());

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_);
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

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential, mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_);
}

TEST_F(ActorLoginCredentialFillerTest, NoSigninForm_NotLoginForm) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com/login"));
  Credential credential = CreateTestCredential(u"username", origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      CreateFormManagerWithParsedForm(
          origin, CreateChangePasswordFormData(origin.GetURL()));
  form_managers.push_back(std::move(form_manager));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential, mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_);
}

TEST_F(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_MultipleCredentials) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const std::u16string username_to_find = u"targetuser";
  const Credential credential =
      CreateTestCredential(username_to_find, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(CreateSavedPasswordForm(origin.GetURL(), u"user1"));
  saved_forms.push_back(CreateSavedPasswordForm(origin.GetURL(), u"user2"));
  form_fetcher_.SetBestMatches(saved_forms);  // No matching username

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential, mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorInvalidCredential)));
  filler.AttemptLogin(&mock_password_manager_);
}

TEST_F(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_NoSavedCredentialsForOrigin) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const std::u16string username = u"testuser";
  const Credential credential = CreateTestCredential(username, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  // No saved forms for this origin (empty vector)
  std::vector<password_manager::PasswordForm> saved_forms;
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential, mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorInvalidCredential)));
  filler.AttemptLogin(&mock_password_manager_);
}

TEST_F(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_SuppliedAndStoredCredentialOriginDiffers) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const std::u16string username = u"testuser";
  const Credential credential =
      CreateTestCredential(username, GURL("https://otherexample.com"));
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  // Prepare a saved credential that does match the requested username, but not
  // the origin
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(CreateSavedPasswordForm(origin.GetURL(), username));
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential, mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorInvalidCredential)));
  filler.AttemptLogin(&mock_password_manager_);
}

}  // namespace actor_login
