// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor_login {

using autofill::FormData;
using autofill::FormFieldData;
using autofill::test::CreateTestFormField;
using base::test::RunOnceCallback;
using device_reauth::DeviceAuthenticator;
using device_reauth::MockDeviceAuthenticator;
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
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;

namespace {

constexpr char16_t kTestUsername[] = u"username";
constexpr char16_t kTestPassword[] = u"password";
constexpr char kLoginUrl[] = "https://example.com/login";

class MockStubPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(const url::Origin&,
              GetLastCommittedOrigin,
              (),
              (const, override));
  MOCK_METHOD(void,
              FillField,
              (autofill::FieldRendererId,
               const std::u16string&,
               autofill::FieldPropertiesFlags,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(PasswordManagerInterface*, GetPasswordManager, (), (override));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(const PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (const, override));
  MOCK_METHOD(bool, IsFillingEnabled, (const GURL&), (const, override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (DeviceAuthenticator*),
              (override));
  MOCK_METHOD(std::unique_ptr<DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
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
  form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  return form;
}

void SetSavedCredential(FakeFormFetcher* form_fetcher,
                        const GURL& url,
                        const std::u16string& username,
                        const std::u16string& password) {
  std::vector<password_manager::PasswordForm> saved_forms;
  PasswordForm form = CreateSavedPasswordForm(url, username, password);
  form_fetcher->SetBestMatches({std::move(form)});
}

MockDeviceAuthenticator* SetUpDeviceAuthenticatorToRequireReauth(
    const MockPasswordManagerClient& client) {
  // Set up the device authenticator and pretend that reauth before
  // filling is required.
  auto mock_device_authenticator = std::make_unique<MockDeviceAuthenticator>();
  MockDeviceAuthenticator* weak_device_authenticator =
      mock_device_authenticator.get();
  EXPECT_CALL(client, GetDeviceAuthenticator)
      .WillOnce(Return(std::move(mock_device_authenticator)));
  EXPECT_CALL(client, IsReauthBeforeFillingRequired).WillOnce(Return(true));
  return weak_device_authenticator;
}

}  // namespace

class ActorLoginCredentialFillerTest : public ::testing::TestWithParam<bool> {
 public:
  ActorLoginCredentialFillerTest() = default;
  ~ActorLoginCredentialFillerTest() override = default;

  void SetUp() override {
    ON_CALL(mock_client_, GetPasswordManager)
        .WillByDefault(Return(&mock_password_manager_));
    ON_CALL(mock_password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&mock_form_cache_));
    ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
    ON_CALL(mock_driver_, GetPasswordManager)
        .WillByDefault(Return(&mock_password_manager_));
    ON_CALL(mock_password_manager_, GetClient())
        .WillByDefault(Return(&mock_client_));
    ON_CALL(mock_client_, IsFillingEnabled).WillByDefault(Return(true));
    ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
        .WillByDefault(Return(false));
    ON_CALL(tab_, IsActivated).WillByDefault(Return(true));
  }

  std::unique_ptr<PasswordFormManager> CreateFormManagerWithParsedForm(
      const url::Origin& origin,
      const autofill::FormData& form_data) {
    ON_CALL(mock_driver_, GetLastCommittedOrigin())
        .WillByDefault(ReturnRef(origin));
    auto form_manager = std::make_unique<PasswordFormManager>(
        &mock_client_, mock_driver_.AsWeakPtr(), form_data, &form_fetcher_,
        std::make_unique<PasswordSaveManagerImpl>(&mock_client_),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_manager->DisableFillingServerPredictionsForTesting();
    form_fetcher_.NotifyFetchCompleted();
    return form_manager;
  }

  bool should_store_permission() const { return GetParam(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  testing::NiceMock<MockPasswordManager> mock_password_manager_;
  testing::NiceMock<MockPasswordFormCache> mock_form_cache_;
  testing::NiceMock<MockPasswordManagerClient> mock_client_;
  MockStubPasswordManagerDriver mock_driver_;
  FakeFormFetcher form_fetcher_;
  tabs::MockTabInterface tab_;
};

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_NoManagers) {
  url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  Credential credential = CreateTestCredential(kTestUsername, origin.GetURL());
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_DifferentOrigin) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com/login"));
  url::Origin other_origin =
      url::Origin::Create(GURL("https://other.com/login"));
  Credential credential = CreateTestCredential(kTestUsername, origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      CreateFormManagerWithParsedForm(
          other_origin, CreateSigninFormData(other_origin.GetURL()));
  form_managers.push_back(std::move(form_manager));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_NoParsedForm) {
  url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  Credential credential = CreateTestCredential(kTestUsername, origin.GetURL());
  FormData form_data = CreateSigninFormData(origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  EXPECT_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillOnce(ReturnRef(origin));
  std::unique_ptr<PasswordFormManager> form_manager =
      std::make_unique<PasswordFormManager>(
          &mock_client_, mock_driver_.AsWeakPtr(), form_data, &form_fetcher_,
          std::make_unique<PasswordSaveManagerImpl>(&mock_client_),
          /*metrics_recorder=*/nullptr);

  form_managers.push_back(std::move(form_manager));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_NotLoginForm) {
  url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  Credential credential = CreateTestCredential(kTestUsername, origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      CreateFormManagerWithParsedForm(
          origin, CreateChangePasswordFormData(origin.GetURL()));
  form_managers.push_back(std::move(form_manager));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback, Run(Eq(LoginStatusResult::kErrorNoSigninForm)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_MultipleCredentials) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(CreateSavedPasswordForm(origin.GetURL(), u"user1"));
  saved_forms.push_back(CreateSavedPasswordForm(origin.GetURL(), u"user2"));
  form_fetcher_.SetBestMatches(saved_forms);  // No matching username

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorInvalidCredential)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_NoSavedCredentialsForOrigin) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  // No saved forms for this origin (empty vector)
  std::vector<password_manager::PasswordForm> saved_forms;
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorInvalidCredential)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_SuppliedAndStoredCredentialOriginDiffers) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const Credential credential =
      CreateTestCredential(kTestUsername, GURL("https://otherexample.com"));
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  // Prepare a saved credential that does match the requested username, but not
  // the origin
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(
      CreateSavedPasswordForm(origin.GetURL(), kTestUsername));
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorInvalidCredential)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

// Tests filling the username and password in a single chosen form.
TEST_P(ActorLoginCredentialFillerTest, FillUsernameAndPasswordSingleForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  EXPECT_FALSE(parsed_form->username_element_renderer_id.is_null());
  EXPECT_FALSE(parsed_form->password_element_renderer_id.is_null());

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest, FillSingleFormStoresPermission) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    base::DoNothing());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  autofill::test_api(form_data).field(0).set_value(kTestUsername);
  autofill::test_api(form_data).field(1).set_value(kTestPassword);
  form_managers[0]->ProvisionallySave(
      form_data, &mock_driver_,
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          /*max_size=*/2));

  EXPECT_EQ(form_managers[0]->GetPendingCredentials().actor_login_approved,
            should_store_permission());
}

// Tests filling the username in a single chosen form.
TEST_P(ActorLoginCredentialFillerTest, FillOnlyUsernameFieldSingleForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());

  // Create a form with only a username field.
  FormData form_data = CreateUsernameOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  EXPECT_FALSE(parsed_form->username_element_renderer_id.is_null());
  EXPECT_TRUE(parsed_form->password_element_renderer_id.is_null());

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);
  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessUsernameFilled);
}

// Tests filling the password in a single chosen form.
TEST_P(ActorLoginCredentialFillerTest, FillOnlyPasswordFieldSingleForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());

  // Create a form with only a password field.
  FormData form_data = CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  EXPECT_TRUE(parsed_form->username_element_renderer_id.is_null());
  EXPECT_FALSE(parsed_form->password_element_renderer_id.is_null());

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessPasswordFilled);
}

// Tests filling the username in a single chosen form.
TEST_P(ActorLoginCredentialFillerTest, FillUsernameFailsSingleForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  filler.AttemptLogin(&mock_password_manager_, tab_);

  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessPasswordFilled);
}

// Tests filling the password in a single chosen form.
TEST_P(ActorLoginCredentialFillerTest, FillPasswordFailsSingleForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessUsernameFilled);
}

// Tests that filling both fields fails in a single chosen form.
TEST_P(ActorLoginCredentialFillerTest, FillBothFailsSingleForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorNoFillableFields);
}

// Tests filling username and password succeeds if filling all eligible fields.
TEST_P(ActorLoginCredentialFillerTest,
       FillUsernameAndPasswordInAllEligibleFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  const FormData username_only_form_data =
      CreateUsernameOnlyFormData(origin.GetURL());
  const FormData password_only_form_data =
      CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, username_only_form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, password_only_form_data));

  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* username_only_parsed_form =
      form_managers[1]->GetParsedObservedForm();
  const PasswordForm* password_only_parsed_form =
      form_managers[2]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // There are 4 fields to fill, so there should be 4 calls to the driver,
  // one for each field. Make the first 2 fail and the last 2 succeed.
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(username_only_parsed_form->username_element_renderer_id,
                Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      mock_driver_,
      FillField(password_only_parsed_form->password_element_renderer_id,
                Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest, StoresPermissionWhenFillingAllFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  FormData form_data = CreateSigninFormData(origin.GetURL());
  FormData username_only_form_data =
      CreateUsernameOnlyFormData(origin.GetURL());
  FormData password_only_form_data =
      CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, username_only_form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, password_only_form_data));
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    base::DoNothing());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  autofill::test_api(form_data).field(0).set_value(kTestUsername);
  autofill::test_api(form_data).field(1).set_value(kTestPassword);
  autofill::test_api(username_only_form_data).field(0).set_value(kTestUsername);
  autofill::test_api(password_only_form_data).field(0).set_value(kTestPassword);
  form_managers[0]->ProvisionallySave(
      form_data, &mock_driver_,
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          /*max_size=*/2));
  // Since there is no password here, it will not produce a parsed form.
  form_managers[1]->ProvisionallySave(
      username_only_form_data, &mock_driver_,
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          /*max_size=*/2));
  form_managers[2]->ProvisionallySave(
      password_only_form_data, &mock_driver_,
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          /*max_size=*/2));

  EXPECT_EQ(form_managers[0]->GetPendingCredentials().actor_login_approved,
            should_store_permission());
  // ProvisionallySave doesn't create parsed password form if there is no
  // password to save.
  EXPECT_EQ(form_managers[1]->GetPendingCredentials().actor_login_approved,
            false);
  EXPECT_EQ(form_managers[2]->GetPendingCredentials().actor_login_approved,
            should_store_permission());
}

TEST_P(ActorLoginCredentialFillerTest, FillOnlyUsernameInAllEligibleFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  const FormData username_only_form_data =
      CreateUsernameOnlyFormData(origin.GetURL());
  const FormData password_only_form_data =
      CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, username_only_form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, password_only_form_data));

  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* username_only_parsed_form =
      form_managers[1]->GetParsedObservedForm();
  const PasswordForm* password_only_parsed_form =
      form_managers[2]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // There are 4 fields to fill, so there should be 4 calls to the driver,
  // one for each field. Make all password fields filling fail and one
  // username filling succeed.
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(username_only_parsed_form->username_element_renderer_id,
                Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      mock_driver_,
      FillField(password_only_parsed_form->password_element_renderer_id,
                Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessUsernameFilled);
}

TEST_P(ActorLoginCredentialFillerTest, FillOnlyPasswordInAllEligibleFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  const FormData username_only_form_data =
      CreateUsernameOnlyFormData(origin.GetURL());
  const FormData password_only_form_data =
      CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, username_only_form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, password_only_form_data));

  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* username_only_parsed_form =
      form_managers[1]->GetParsedObservedForm();
  const PasswordForm* password_only_parsed_form =
      form_managers[2]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // There are 4 fields to fill, so there should be 4 calls to the driver,
  // one for each field. Make all username fields filling fail and one
  // password filling succeed.
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      mock_driver_,
      FillField(username_only_parsed_form->username_element_renderer_id,
                Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(password_only_parsed_form->password_element_renderer_id,
                Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest, FillingFailsInAllEligibleFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  const FormData username_only_form_data =
      CreateUsernameOnlyFormData(origin.GetURL());
  const FormData password_only_form_data =
      CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, username_only_form_data));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, password_only_form_data));

  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* username_only_parsed_form =
      form_managers[1]->GetParsedObservedForm();
  const PasswordForm* password_only_parsed_form =
      form_managers[2]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // There are 4 fields to fill, so there should be 4 calls to the driver,
  // one for each field. Make all filling attempts fail.
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(username_only_parsed_form->username_element_renderer_id,
                Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      mock_driver_,
      FillField(password_only_parsed_form->password_element_renderer_id,
                Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorNoFillableFields);
}

TEST_P(ActorLoginCredentialFillerTest, FillingIsDisabled) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const Credential credential =
      CreateTestCredential(u"username", origin.GetURL());

  EXPECT_CALL(mock_client_, IsFillingEnabled(origin.GetURL()))
      .WillOnce(Return(false));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorFillingNotAllowed)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest, RequestsReauthBeforeFillingSingleForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(parsed_form));

  MockDeviceAuthenticator* weak_device_authenticator =
      SetUpDeviceAuthenticatorToRequireReauth(mock_client_);

  // Check that the authenticator is invoked before filling.
  // Simulate successful reauth.
  EXPECT_CALL(*weak_device_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest, RequestsReauthBeforeFillingAllFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  MockDeviceAuthenticator* weak_device_authenticator =
      SetUpDeviceAuthenticatorToRequireReauth(mock_client_);

  // Check that the authenticator is invoked before filling.
  // Simulate successful reauth.
  EXPECT_CALL(*weak_device_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest, TabNotActive_ReturnsErrorBeforeReauth) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));

  SetUpDeviceAuthenticatorToRequireReauth(mock_client_);

  EXPECT_CALL(tab_, IsActivated).WillOnce(Return(false));

  filler.AttemptLogin(&mock_password_manager_, tab_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorDeviceReauthRequired);
}

TEST_P(ActorLoginCredentialFillerTest, DoesntFillIfReauthFails) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  // Set up the device authenticator and pretend that reauth before
  // filling is required.
  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));

  MockDeviceAuthenticator* weak_device_authenticator =
      SetUpDeviceAuthenticatorToRequireReauth(mock_client_);

  // Check that the authenticator is invoked before filling.
  // Simulate failed reauth.
  EXPECT_CALL(*weak_device_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorDeviceReauthFailed)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

TEST_P(ActorLoginCredentialFillerTest, ReturnsErrorIfFormWentAwayDuringReauth) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFillingHeuristics);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL());
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  ActorLoginCredentialFiller filler(origin, credential,
                                    should_store_permission(), &mock_client_,
                                    mock_callback.Get());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));

  MockDeviceAuthenticator* weak_device_authenticator =
      SetUpDeviceAuthenticatorToRequireReauth(mock_client_);

  // Check that the authenticator is invoked before filling.
  // Simulate failed reauth.
  EXPECT_CALL(*weak_device_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));

  // Pretend that the parsed form went away during the reauth.
  EXPECT_CALL(
      mock_form_cache_,
      GetPasswordForm(&mock_driver_, parsed_form->form_data.renderer_id()))
      .WillOnce(Return(nullptr));

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);

  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);
  EXPECT_CALL(mock_callback,
              Run(Eq(LoginStatusResult::kErrorNoFillableFields)));
  filler.AttemptLogin(&mock_password_manager_, tab_);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorLoginCredentialFillerTest,
                         ::testing::Bool());

}  // namespace actor_login
