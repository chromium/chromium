// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_quality_logger.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/proto_extras/proto_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor_login {

using autofill::FormData;
using autofill::FormFieldData;
using autofill::test::CreateTestFormField;
using base::test::EqualsProto;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
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
using testing::Matcher;
using testing::Return;
using testing::ReturnRef;
using testing::WithArg;

namespace {

using AttemptLoginDetails =
    optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails;
using FillingFormResult = optimization_guide::proto::
    ActorLoginQuality_AttemptLoginDetails_FillingFormResult;
using ParsedFormDetails =
    optimization_guide::proto::ActorLoginQuality_ParsedFormDetails;
using FieldData =
    optimization_guide::proto::ActorLoginQuality_FormData_FieldData;

Matcher<AttemptLoginDetails> EqualsAttemptLoginDetails(
    const AttemptLoginDetails& expected) {
  std::vector<testing::Matcher<const FillingFormResult&>> form_result_matchers;
  form_result_matchers.reserve(expected.filling_form_result().size());
  for (const auto& result : expected.filling_form_result()) {
    form_result_matchers.push_back(EqualsProto(result));
  }

  std::vector<testing::Matcher<const ParsedFormDetails&>> parsed_form_matchers;
  parsed_form_matchers.reserve(expected.parsed_form_details().size());
  for (const auto& detail : expected.parsed_form_details()) {
    parsed_form_matchers.push_back(EqualsProto(detail));
  }

  return testing::AllOf(
      testing::Property("outcome", &AttemptLoginDetails::outcome,
                        expected.outcome()),
      testing::Property("attempt_login_time_ms",
                        &AttemptLoginDetails::attempt_login_time_ms,
                        expected.attempt_login_time_ms()),
      testing::Property(
          "filling_form_result", &AttemptLoginDetails::filling_form_result,
          testing::UnorderedElementsAreArray(form_result_matchers)),
      testing::Property(
          "parsed_form_details", &AttemptLoginDetails::parsed_form_details,
          testing::UnorderedElementsAreArray(parsed_form_matchers)));
}

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
  MOCK_METHOD(bool, IsDirectChildOfPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsNestedWithinFencedFrame, (), (const, override));
  MOCK_METHOD(PasswordManagerInterface*, GetPasswordManager, (), (override));
  MOCK_METHOD(void,
              CheckViewAreaVisible,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
              (override));
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

template <bool success>
void PostResponse(base::OnceCallback<void(bool)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void SetSavedCredential(FakeFormFetcher* form_fetcher,
                        const GURL& url,
                        const std::u16string& username,
                        const std::u16string& password) {
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
    ON_CALL(mock_driver_, IsDirectChildOfPrimaryMainFrame)
        .WillByDefault(Return(true));
    ON_CALL(mock_driver_, GetPasswordManager)
        .WillByDefault(Return(&mock_password_manager_));
    ON_CALL(mock_driver_, GetLastCommittedOrigin())
        .WillByDefault(ReturnRef(main_frame_origin_));
    // Assume that by default all fields are visible.
    ON_CALL(mock_driver_, CheckViewAreaVisible)
        .WillByDefault(WithArg<1>(&PostResponse<true>));

    ON_CALL(mock_password_manager_, GetClient())
        .WillByDefault(Return(&mock_client_));
    ON_CALL(mock_client_, IsFillingEnabled).WillByDefault(Return(true));
    ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
        .WillByDefault(Return(false));
  }

  base::WeakPtr<MockActorLoginQualityLogger> mqls_logger() {
    return mock_mqls_logger_.AsWeakPtr();
  }

  std::unique_ptr<PasswordFormManager> CreateFormManagerWithParsedForm(
      const url::Origin& origin,
      const autofill::FormData& form_data) {
    return CreateFormManagerWithParsedForm(origin, form_data, mock_driver_,
                                           form_fetcher_);
  }

  std::unique_ptr<PasswordFormManager> CreateFormManagerWithParsedForm(
      const url::Origin& origin,
      const autofill::FormData& form_data,
      MockStubPasswordManagerDriver& mock_driver) {
    return CreateFormManagerWithParsedForm(origin, form_data, mock_driver,
                                           form_fetcher_);
  }

  std::unique_ptr<PasswordFormManager> CreateFormManagerWithParsedForm(
      const url::Origin& origin,
      const autofill::FormData& form_data,
      MockStubPasswordManagerDriver& mock_driver,
      FakeFormFetcher& form_fetcher) {
    ON_CALL(mock_driver, GetLastCommittedOrigin())
        .WillByDefault(ReturnRef(origin));
    ON_CALL(mock_driver, IsInPrimaryMainFrame)
        .WillByDefault(Return(origin.IsSameOriginWith(main_frame_origin_)));
    ON_CALL(mock_driver, GetPasswordManager)
        .WillByDefault(Return(&mock_password_manager_));
    auto form_manager = std::make_unique<PasswordFormManager>(
        &mock_client_, mock_driver.AsWeakPtr(), form_data, &form_fetcher,
        std::make_unique<PasswordSaveManagerImpl>(&mock_client_),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_manager->DisableFillingServerPredictionsForTesting();
    form_fetcher.NotifyFetchCompleted();
    return form_manager;
  }

  bool should_store_permission() const { return GetParam(); }

 protected:
  url::Origin main_frame_origin_ =
      url::Origin::Create(GURL("https://example.com"));

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  testing::NiceMock<MockPasswordManager> mock_password_manager_;
  testing::NiceMock<MockPasswordFormCache> mock_form_cache_;
  base::MockCallback<ActorLoginCredentialFiller::IsTaskInFocus>
      mock_is_task_in_focus_;
  testing::NiceMock<MockPasswordManagerClient> mock_client_;
  MockStubPasswordManagerDriver mock_driver_;
  FakeFormFetcher form_fetcher_;
  MockActorLoginQualityLogger mock_mqls_logger_;
};

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_NoManagers) {
  url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;

  base::test::TestFuture<LoginStatusResultOrError> future;
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  filler->AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_NO_SIGN_IN_FORM);
  expected_details.set_attempt_login_time_ms(0);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_CrossSiteIframe) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com/login"));
  url::Origin cross_site_origin =
      url::Origin::Create(GURL("https://other.com/login"));
  Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      CreateFormManagerWithParsedForm(
          cross_site_origin, CreateSigninFormData(cross_site_origin.GetURL()));
  form_managers.push_back(std::move(form_manager));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));
  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_NoParsedForm) {
  url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  FormData form_data = CreateSigninFormData(origin.GetURL());

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      std::make_unique<PasswordFormManager>(
          &mock_client_, mock_driver_.AsWeakPtr(), form_data, &form_fetcher_,
          std::make_unique<PasswordSaveManagerImpl>(&mock_client_),
          /*metrics_recorder=*/nullptr);

  form_managers.push_back(std::move(form_manager));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));
  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_P(ActorLoginCredentialFillerTest, NoSigninForm_NotLoginForm) {
  url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  std::unique_ptr<PasswordFormManager> form_manager =
      CreateFormManagerWithParsedForm(
          origin, CreateChangePasswordFormData(origin.GetURL()));
  form_managers.push_back(std::move(form_manager));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_P(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_MultipleCredentials) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(CreateSavedPasswordForm(origin.GetURL(), u"user1"));
  saved_forms.push_back(CreateSavedPasswordForm(origin.GetURL(), u"user2"));
  form_fetcher_.SetBestMatches(saved_forms);  // No matching username

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::test::TestFuture<LoginStatusResultOrError> future;
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  filler->AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorInvalidCredential);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_INVALID_CREDENTIAL);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *form_managers[0]->GetParsedObservedForm(), /*is_username_visible=*/true,
      /*is_password_visible=*/true);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest,
       CredentialNotSavedForOrigin_NoSavedCredentialsForOrigin) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  // No saved forms for this origin (empty vector)
  std::vector<password_manager::PasswordForm> saved_forms;
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));
  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorInvalidCredential);
}

TEST_P(
    ActorLoginCredentialFillerTest,
    CredentialNotSavedForOrigin_SuppliedAndStoredCredentialSavedSiteDiffers) {
  const url::Origin request_origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const Credential credential = CreateTestCredential(
      kTestUsername, GURL("https://otherexample.com"), request_origin);
  const FormData form_data = CreateSigninFormData(request_origin.GetURL());
  // Prepare a saved credential that does match the requested username, but
  // not the site it was saved on.
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(
      CreateSavedPasswordForm(request_origin.GetURL(), kTestUsername));
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(request_origin, form_data));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      request_origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorInvalidCredential);
}

TEST_P(ActorLoginCredentialFillerTest,
       InvalidCredential_SuppliedCredentialRequestOriginDiffers) {
  const url::Origin main_frame_origin =
      url::Origin::Create(GURL("https://example.com/login"));
  ON_CALL(mock_driver_, GetLastCommittedOrigin)
      .WillByDefault(ReturnRef(main_frame_origin));

  // The credential was requested on a different origin than the current one.
  const url::Origin request_origin =
      url::Origin::Create(GURL("https://otherexample.com"));
  const Credential credential = CreateTestCredential(
      kTestUsername, request_origin.GetURL(), request_origin);

  // Prepare a saved credential for the form.
  const FormData form_data = CreateSigninFormData(main_frame_origin.GetURL());
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(
      CreateSavedPasswordForm(main_frame_origin.GetURL(), kTestUsername));
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(main_frame_origin, form_data));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      main_frame_origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorInvalidCredential);
}

TEST_P(ActorLoginCredentialFillerTest, DoesntFillFencedFrameForm) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(
      CreateSavedPasswordForm(origin.GetURL(), kTestUsername));
  form_fetcher_.SetBestMatches(saved_forms);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(false));
  EXPECT_CALL(mock_driver_, IsNestedWithinFencedFrame).WillOnce(Return(true));

  filler.AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_P(ActorLoginCredentialFillerTest,
       DoesntFillNestedFrameWithDifferentOrigin) {
  const url::Origin main_frame_origin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin form_origin =
      url::Origin::Create(GURL("https://other.com"));
  const Credential credential = CreateTestCredential(
      kTestUsername, main_frame_origin.GetURL(), main_frame_origin);
  const FormData form_data = CreateSigninFormData(form_origin.GetURL());
  SetSavedCredential(&form_fetcher_, main_frame_origin.GetURL(), kTestUsername,
                     kTestPassword);

  ON_CALL(mock_driver_, GetLastCommittedOrigin)
      .WillByDefault(ReturnRef(form_origin));

  // Neither the form frame or its parent are the primary main frame.
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(false));
  EXPECT_CALL(mock_driver_, IsDirectChildOfPrimaryMainFrame)
      .WillRepeatedly(Return(false));

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(form_origin, form_data, mock_driver_));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ;
  ActorLoginCredentialFiller filler(
      main_frame_origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_P(ActorLoginCredentialFillerTest, FillsNestedFrameWithSameOrigin) {
  const url::Origin main_frame_origin =
      url::Origin::Create(GURL("https://example.com"));
  const Credential credential = CreateTestCredential(
      kTestUsername, main_frame_origin.GetURL(), main_frame_origin);
  const FormData form_data = CreateSigninFormData(main_frame_origin.GetURL());
  SetSavedCredential(&form_fetcher_, main_frame_origin.GetURL(), kTestUsername,
                     kTestPassword);

  // The form frame's last committed origin is the same as the main frame
  // origin.
  ON_CALL(mock_driver_, GetLastCommittedOrigin)
      .WillByDefault(ReturnRef(main_frame_origin));

  // Neither the form frame or its parent are in the main frame.
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(false));
  EXPECT_CALL(mock_driver_, IsDirectChildOfPrimaryMainFrame)
      .WillRepeatedly(Return(false));

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(
      main_frame_origin, form_data, mock_driver_));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      main_frame_origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));
  EXPECT_CALL(mock_driver_,
              FillField(parsed_form->username_element_renderer_id, _, _, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(mock_driver_,
              FillField(parsed_form->password_element_renderer_id, _, _, _))
      .WillOnce(RunOnceCallback<3>(true));

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/true, /*is_password_visible=*/true);
  FillingFormResult* form_result = expected_details.add_filling_form_result();
  *form_result->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result->set_was_username_filled(true);
  form_result->set_was_password_filled(true);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest,
       FillsSameSiteDirectChildOfPrimaryMainFrame) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {password_manager::features::kActorLoginSameSiteIframeSupport}, {});

  const url::Origin main_frame_origin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin form_origin =
      url::Origin::Create(GURL("https://login.example.com"));
  const Credential credential = CreateTestCredential(
      kTestUsername, form_origin.GetURL(), main_frame_origin);
  const FormData form_data = CreateSigninFormData(form_origin.GetURL());
  SetSavedCredential(&form_fetcher_, form_origin.GetURL(), kTestUsername,
                     kTestPassword);

  ON_CALL(mock_driver_, GetLastCommittedOrigin)
      .WillByDefault(ReturnRef(form_origin));

  // Form is not in the main frame, but in the direct child of the main frame.
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(false));

  EXPECT_CALL(mock_driver_, IsDirectChildOfPrimaryMainFrame)
      .WillRepeatedly(Return(true));
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(form_origin, form_data, mock_driver_));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      main_frame_origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  EXPECT_CALL(mock_driver_,
              FillField(parsed_form->username_element_renderer_id, _, _, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(mock_driver_,
              FillField(parsed_form->password_element_renderer_id, _, _, _))
      .WillOnce(RunOnceCallback<3>(true));

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest,
       DoesntFillSameSiteDirectChildOfPrimaryMainFrame_FeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {password_manager::features::kActorLoginSameSiteIframeSupport});

  const url::Origin main_frame_origin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin form_origin =
      url::Origin::Create(GURL("https://login.example.com"));
  const Credential credential = CreateTestCredential(
      kTestUsername, form_origin.GetURL(), main_frame_origin);
  const FormData form_data = CreateSigninFormData(form_origin.GetURL());
  SetSavedCredential(&form_fetcher_, form_origin.GetURL(), kTestUsername,
                     kTestPassword);

  ON_CALL(mock_driver_, GetLastCommittedOrigin)
      .WillByDefault(ReturnRef(form_origin));
  // Form is not in the main frame, but in a frame that is the direct child
  // of the main frame.
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(false));
  EXPECT_CALL(mock_driver_, IsDirectChildOfPrimaryMainFrame)
      .WillRepeatedly(Return(true));

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(form_origin, form_data, mock_driver_));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      main_frame_origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));

  EXPECT_FALSE(parsed_form->username_element_renderer_id.is_null());
  EXPECT_FALSE(parsed_form->password_element_renderer_id.is_null());

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_P(ActorLoginCredentialFillerTest, DoesntFillSameSiteNestedIframe) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {password_manager::features::kActorLoginSameSiteIframeSupport}, {});
  const url::Origin origin = url::Origin::Create(GURL("https://example.com/"));
  const url::Origin same_site_origin =
      url::Origin::Create(GURL("https://login.example.com"));
  const Credential credential =
      CreateTestCredential(kTestUsername, same_site_origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(same_site_origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, same_site_origin.GetURL(), kTestUsername,
                     kTestPassword);

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(same_site_origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillOnce(Return(base::span(form_managers)));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(false));
  EXPECT_CALL(mock_driver_, IsNestedWithinFencedFrame).WillOnce(Return(false));
  EXPECT_CALL(mock_driver_, IsDirectChildOfPrimaryMainFrame)
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(parsed_form->username_element_renderer_id.is_null());
  EXPECT_FALSE(parsed_form->password_element_renderer_id.is_null());

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorNoSigninForm);
}

// Tests filling username and password succeeds if filling all eligible
// fields.
TEST_P(ActorLoginCredentialFillerTest,
       FillUsernameAndPasswordInAllEligibleFields) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
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
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

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

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/true, /*is_password_visible=*/true);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *username_only_parsed_form, /*is_username_visible=*/true,
      /*is_password_visible=*/false);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *password_only_parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/true);

  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result1->set_was_username_filled(false);
  form_result1->set_was_password_filled(false);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() =
      CreateExpectedFormData(*username_only_parsed_form);
  form_result2->set_was_username_filled(true);
  FillingFormResult* form_result3 = expected_details.add_filling_form_result();
  *form_result3->mutable_form_data() =
      CreateExpectedFormData(*password_only_parsed_form);
  form_result3->set_was_password_filled(true);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest,
       FillUsernameAndPasswordInAllEligibleFieldsAcrossSameSiteIframes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginSameSiteIframeSupport);

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const url::Origin same_site_origin_1 =
      url::Origin::Create(GURL("https://login.example.com"));
  const url::Origin same_site_origin_2 =
      url::Origin::Create(GURL("https://login2.example.com"));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData same_site_form_data_1 =
      CreateSigninFormData(same_site_origin_1.GetURL());
  const FormData same_site_form_data_2 =
      CreateSigninFormData(same_site_origin_2.GetURL());

  // Make sure a saved credential with a matching username exists for both
  // forms.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  MockStubPasswordManagerDriver same_site_driver_1;
  MockStubPasswordManagerDriver same_site_driver_2;
  form_managers.push_back(CreateFormManagerWithParsedForm(
      same_site_origin_1, same_site_form_data_1, same_site_driver_1));
  form_managers.push_back(CreateFormManagerWithParsedForm(
      same_site_origin_2, same_site_form_data_2, same_site_driver_2));

  const PasswordForm* parsed_form_1 = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* parsed_form_2 = form_managers[1]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // Set up the iframes to be direct children of the primary main frame.
  ON_CALL(same_site_driver_1, IsNestedWithinFencedFrame)
      .WillByDefault(Return(false));
  ON_CALL(same_site_driver_1, IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));
  ON_CALL(same_site_driver_2, IsNestedWithinFencedFrame)
      .WillByDefault(Return(false));
  ON_CALL(same_site_driver_2, IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));

  // Make all field visibility checks return true;
  ON_CALL(same_site_driver_1, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  ON_CALL(same_site_driver_2, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));

  // There are 4 fields to fill, so there should be 4 calls to the driver,
  // one for each field. Make the first 2 fail and the last 2 succeed.
  EXPECT_CALL(
      same_site_driver_1,
      FillField(parsed_form_1->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      same_site_driver_1,
      FillField(parsed_form_1->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(false));
  EXPECT_CALL(
      same_site_driver_2,
      FillField(parsed_form_2->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      same_site_driver_2,
      FillField(parsed_form_2->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form_1, /*is_username_visible=*/true,
      /*is_password_visible=*/true);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form_2, /*is_username_visible=*/true,
      /*is_password_visible=*/true);

  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form_1);
  form_result1->set_was_username_filled(false);
  form_result1->set_was_password_filled(false);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() = CreateExpectedFormData(*parsed_form_2);
  form_result2->set_was_username_filled(true);
  form_result2->set_was_password_filled(true);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest,
       DoesntFillUsernameAndPasswordThatAreNotBestMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginSameSiteIframeSupport);
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  main_frame_origin_ = origin;
  const url::Origin same_site_origin_1 =
      url::Origin::Create(GURL("https://login.example.com"));
  const url::Origin same_site_origin_2 =
      url::Origin::Create(GURL("https://login2.example.com"));
  const Credential credential =
      CreateTestCredential(kTestUsername, same_site_origin_1.GetURL(), origin);
  const FormData same_site_form_data_1 =
      CreateSigninFormData(same_site_origin_1.GetURL());
  const FormData same_site_form_data_2 =
      CreateSigninFormData(same_site_origin_2.GetURL());
  FakeFormFetcher empty_form_fetcher;

  // Make sure a saved credential with a matching username exists for
  // `same_site_origin1` which will be tied to `form_fetcher_`.
  SetSavedCredential(&form_fetcher_, same_site_origin_1.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  MockStubPasswordManagerDriver same_site_driver_1;
  MockStubPasswordManagerDriver same_site_driver_2;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(same_site_origin_1, same_site_form_data_1,
                                      same_site_driver_1, form_fetcher_));
  form_managers.push_back(
      CreateFormManagerWithParsedForm(same_site_origin_2, same_site_form_data_2,
                                      same_site_driver_2, empty_form_fetcher));

  const PasswordForm* parsed_form_1 = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* parsed_form_2 = form_managers[1]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // Set up the iframes to be direct children of the primary main frame.
  ON_CALL(same_site_driver_1, IsNestedWithinFencedFrame)
      .WillByDefault(Return(false));
  ON_CALL(same_site_driver_1, IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));
  ON_CALL(same_site_driver_2, IsNestedWithinFencedFrame)
      .WillByDefault(Return(false));
  ON_CALL(same_site_driver_2, IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));

  // Make sure field visibility calls return true for fields in both iframes.
  ON_CALL(same_site_driver_1, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  ON_CALL(same_site_driver_2, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));

  // The first form manager has a matching best_match, fill the corresponding
  // form.
  EXPECT_CALL(
      same_site_driver_1,
      FillField(parsed_form_1->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      same_site_driver_1,
      FillField(parsed_form_1->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  // The second form manager doesn't have best matches, don't fill the
  // corresponding form.
  EXPECT_CALL(
      same_site_driver_2,
      FillField(parsed_form_2->username_element_renderer_id, _,
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);
  EXPECT_CALL(
      same_site_driver_2,
      FillField(parsed_form_2->password_element_renderer_id, _,
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest,
       FillUsernameAndPasswordInAllEligibleFieldsPreferMainframe) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginSameSiteIframeSupport);
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const url::Origin same_site_origin_1 =
      url::Origin::Create(GURL("https://login.example.com"));
  const url::Origin same_site_origin_2 =
      url::Origin::Create(GURL("https://login2.example.com"));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  const FormData same_site_form_data_1 =
      CreateSigninFormData(same_site_origin_1.GetURL());
  const FormData same_site_form_data_2 =
      CreateSigninFormData(same_site_origin_2.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate signin forms existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  MockStubPasswordManagerDriver mock_iframe_driver_1;
  form_managers.push_back(CreateFormManagerWithParsedForm(
      same_site_origin_1, same_site_form_data_1, mock_iframe_driver_1));
  MockStubPasswordManagerDriver mock_iframe_driver_2;
  form_managers.push_back(CreateFormManagerWithParsedForm(
      same_site_origin_2, same_site_form_data_2, mock_iframe_driver_2));

  const PasswordForm* parsed_form_1 = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* parsed_form_2 = form_managers[1]->GetParsedObservedForm();
  const PasswordForm* parsed_form_3 = form_managers[2]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // Set up the iframes to be direct children of the primary main frame.
  ON_CALL(mock_iframe_driver_1, IsNestedWithinFencedFrame)
      .WillByDefault(Return(false));
  ON_CALL(mock_iframe_driver_1, IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));
  ON_CALL(mock_iframe_driver_2, IsNestedWithinFencedFrame)
      .WillByDefault(Return(false));
  ON_CALL(mock_iframe_driver_2, IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));

  // Make sure field visibility calls return true for fields in the iframes too.
  // The main frame is set up by default with visible fields.
  ON_CALL(mock_iframe_driver_1, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  ON_CALL(mock_iframe_driver_2, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));

  // There are 6 fields to fill but only 2 are in mainframe. Fill the fields
  // the mainframe
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form_1->username_element_renderer_id, Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(
      mock_driver_,
      FillField(parsed_form_1->password_element_renderer_id, Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(mock_iframe_driver_1,
              FillField(parsed_form_2->username_element_renderer_id, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_iframe_driver_1,
              FillField(parsed_form_2->password_element_renderer_id, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_iframe_driver_2,
              FillField(parsed_form_3->username_element_renderer_id, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_iframe_driver_2,
              FillField(parsed_form_3->password_element_renderer_id, _, _, _))
      .Times(0);

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest, StoresPermissionWhenFillingAllFields) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
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

  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();
  const PasswordForm* username_only_parsed_form =
      form_managers[1]->GetParsedObservedForm();
  const PasswordForm* password_only_parsed_form =
      form_managers[2]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));

  // There are 4 fields to fill, make them all succeed.
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

  filler->AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());

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
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/true, /*is_password_visible=*/true);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *username_only_parsed_form, /*is_username_visible=*/true,
      /*is_password_visible=*/false);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *password_only_parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/true);

  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result1->set_was_username_filled(true);
  form_result1->set_was_password_filled(true);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() =
      CreateExpectedFormData(*username_only_parsed_form);
  form_result2->set_was_username_filled(true);
  FillingFormResult* form_result3 = expected_details.add_filling_form_result();
  *form_result3->mutable_form_data() =
      CreateExpectedFormData(*password_only_parsed_form);
  form_result3->set_was_password_filled(true);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest, FillOnlyUsernameInAllEligibleFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          password_manager::features::kActorLoginFieldVisibilityCheck});
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
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
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

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

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessUsernameFilled);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);

  // The async check is not executed, so there are only details
  // about the form data.
  *expected_details.add_parsed_form_details()->mutable_form_data() =
      CreateExpectedFormData(*parsed_form);
  *expected_details.add_parsed_form_details()->mutable_form_data() =
      CreateExpectedFormData(*username_only_parsed_form);
  *expected_details.add_parsed_form_details()->mutable_form_data() =
      CreateExpectedFormData(*password_only_parsed_form);

  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result1->set_was_username_filled(false);
  form_result1->set_was_password_filled(false);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() =
      CreateExpectedFormData(*username_only_parsed_form);
  form_result2->set_was_username_filled(true);
  FillingFormResult* form_result3 = expected_details.add_filling_form_result();
  *form_result3->mutable_form_data() =
      CreateExpectedFormData(*password_only_parsed_form);
  form_result3->set_was_password_filled(false);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest, FillOnlyPasswordInAllEligibleFields) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
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
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

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

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kSuccessPasswordFilled);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/true, /*is_password_visible=*/true);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *username_only_parsed_form, /*is_username_visible=*/true,
      /*is_password_visible=*/false);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *password_only_parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/true);

  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result1->set_was_username_filled(false);
  form_result1->set_was_password_filled(true);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() =
      CreateExpectedFormData(*username_only_parsed_form);
  form_result2->set_was_username_filled(false);
  FillingFormResult* form_result3 = expected_details.add_filling_form_result();
  *form_result3->mutable_form_data() =
      CreateExpectedFormData(*password_only_parsed_form);
  form_result3->set_was_password_filled(false);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest, FillingFailsInAllEligibleFields) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
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
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());

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

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorNoFillableFields);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_NO_FILLABLE_FIELDS);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/true, /*is_password_visible=*/true);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *username_only_parsed_form, /*is_username_visible=*/true,
      /*is_password_visible=*/false);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *password_only_parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/true);

  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result1->set_was_username_filled(false);
  form_result1->set_was_password_filled(false);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() =
      CreateExpectedFormData(*username_only_parsed_form);
  form_result2->set_was_username_filled(false);
  FillingFormResult* form_result3 = expected_details.add_filling_form_result();
  *form_result3->mutable_form_data() =
      CreateExpectedFormData(*password_only_parsed_form);
  form_result3->set_was_password_filled(false);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest, FillingIsDisabled) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/login"));
  const Credential credential =
      CreateTestCredential(u"username", origin.GetURL(), origin);

  EXPECT_CALL(mock_client_, IsFillingEnabled(origin.GetURL()))
      .WillOnce(Return(false));

  base::MockCallback<LoginStatusResultOrErrorReply> mock_callback;
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(Eq(base::unexpected(ActorLoginError::kFillingNotAllowed))));

  filler->AttemptLogin(&mock_password_manager_);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_UNSPECIFIED);
  expected_details.set_attempt_login_time_ms(0);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest, RequestsReauthBeforeFillingAllFields) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ON_CALL(mock_is_task_in_focus_, Run).WillByDefault(Return(true));
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
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

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
}

TEST_P(ActorLoginCredentialFillerTest,
       FillAllFields_OnlyUsernamesVisible_AsyncVisibilityCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFieldVisibilityCheck);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  const FormData username_only_form_data =
      CreateUsernameOnlyFormData(origin.GetURL());
  const FormData password_only_form_data =
      CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

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
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));
  EXPECT_CALL(mock_driver_, CheckViewAreaVisible(
                                parsed_form->username_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<true>));
  EXPECT_CALL(mock_driver_, CheckViewAreaVisible(
                                parsed_form->password_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<false>));
  EXPECT_CALL(mock_driver_,
              CheckViewAreaVisible(
                  username_only_parsed_form->username_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<true>));
  EXPECT_CALL(mock_driver_,
              CheckViewAreaVisible(
                  password_only_parsed_form->password_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<false>));

  // There are 4 fields to fill, in 3 forms. The visibility checks only affect
  // the form classification as a sign in form.
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
      .Times(0);

  const int kRequestDurationMs = 5;
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(kRequestDurationMs);
  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result1->set_was_username_filled(true);
  form_result1->set_was_password_filled(true);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() =
      CreateExpectedFormData(*username_only_parsed_form);
  form_result2->set_was_username_filled(true);

  // Expect parsed form details for all 3 forms found.
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/true,
      /*is_password_visible=*/false, kRequestDurationMs);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *username_only_parsed_form, /*is_username_visible=*/true,
      /*is_password_visible=*/false, kRequestDurationMs);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *password_only_parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/false, kRequestDurationMs);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));

  filler->AttemptLogin(&mock_password_manager_);
  task_environment_.AdvanceClock(base::Milliseconds(kRequestDurationMs));
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest,
       FillAllFields_OnlyPasswordsVisible_AsyncVisibilityCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFieldVisibilityCheck);

  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  const FormData username_only_form_data =
      CreateUsernameOnlyFormData(origin.GetURL());
  const FormData password_only_form_data =
      CreatePasswordOnlyFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

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
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  ON_CALL(mock_form_cache_, GetFormManagers)
      .WillByDefault(Return(base::span(form_managers)));
  EXPECT_CALL(mock_driver_, CheckViewAreaVisible(
                                parsed_form->username_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<false>));
  EXPECT_CALL(mock_driver_, CheckViewAreaVisible(
                                parsed_form->password_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<true>));
  EXPECT_CALL(mock_driver_,
              CheckViewAreaVisible(
                  username_only_parsed_form->username_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<false>));
  EXPECT_CALL(mock_driver_,
              CheckViewAreaVisible(
                  password_only_parsed_form->password_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<true>));

  // There are 4 fields to fill, in 3 forms. For now, the visibility checks only
  // affect the form classification as a sign in form.
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
  EXPECT_CALL(
      mock_driver_,
      FillField(username_only_parsed_form->username_element_renderer_id,
                Eq(kTestUsername),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .Times(0);
  EXPECT_CALL(
      mock_driver_,
      FillField(password_only_parsed_form->password_element_renderer_id,
                Eq(kTestPassword),
                autofill::FieldPropertiesFlags::kAutofilledActorLogin, _))
      .WillOnce(RunOnceCallback<3>(true));

  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS);
  expected_details.set_attempt_login_time_ms(0);
  FillingFormResult* form_result1 = expected_details.add_filling_form_result();
  *form_result1->mutable_form_data() = CreateExpectedFormData(*parsed_form);
  form_result1->set_was_username_filled(true);
  form_result1->set_was_password_filled(true);
  FillingFormResult* form_result2 = expected_details.add_filling_form_result();
  *form_result2->mutable_form_data() =
      CreateExpectedFormData(*password_only_parsed_form);
  form_result2->set_was_password_filled(true);

  // Expect parsed form details for all 3 forms found.
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/true);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *username_only_parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/false);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *password_only_parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/true);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(),
            LoginStatusResult::kSuccessUsernameAndPasswordFilled);
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest,
       UsernameAndPasswordFieldAreNotVisible_AsyncCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFieldVisibilityCheck);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));
  const PasswordForm* parsed_form = form_managers[0]->GetParsedObservedForm();

  base::test::TestFuture<LoginStatusResultOrError> future;
  ActorLoginCredentialFiller filler(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));
  EXPECT_CALL(mock_driver_, CheckViewAreaVisible(
                                parsed_form->username_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<false>));
  EXPECT_CALL(mock_driver_, CheckViewAreaVisible(
                                parsed_form->password_element_renderer_id, _))
      .WillOnce(WithArg<1>(&PostResponse<false>));

  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_NO_SIGN_IN_FORM);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/false,
      /*is_password_visible=*/false);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));

  filler.AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_P(ActorLoginCredentialFillerTest,
       TaskNotInFocus_ReturnsErrorBeforeReauth) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginReauthTaskRefocus);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::test::TestFuture<LoginStatusResultOrError> future;
  EXPECT_CALL(mock_is_task_in_focus_, Run).WillOnce(Return(false));
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  SetUpDeviceAuthenticatorToRequireReauth(mock_client_);

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorDeviceReauthRequired);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_REAUTH_REQUIRED);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *form_managers[0]->GetParsedObservedForm(), /*is_username_visible=*/true,
      /*is_password_visible=*/true);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest,
       TaskNotFocused_NoErrorBeforeReauthIfFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginReauthTaskRefocus);
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());

  // Make sure a saved credential with a matching username exists.
  SetSavedCredential(&form_fetcher_, origin.GetURL(), kTestUsername,
                     kTestPassword);

  // Simulate a signin form existing on the page.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(CreateFormManagerWithParsedForm(origin, form_data));

  base::test::TestFuture<LoginStatusResultOrError> future;
  ON_CALL(mock_is_task_in_focus_, Run).WillByDefault(Return(false));
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

  MockDeviceAuthenticator* weak_device_authenticator =
      SetUpDeviceAuthenticatorToRequireReauth(mock_client_);

  // Check that the authenticator is invoked before filling.
  // Simulate a failed re-auth since we're not interested in the rest of
  // the flow.
  EXPECT_CALL(*weak_device_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));

  filler->AttemptLogin(&mock_password_manager_);
  const LoginStatusResultOrError& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), LoginStatusResult::kErrorDeviceReauthFailed);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_REAUTH_FAILED);
  expected_details.set_attempt_login_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *form_managers[0]->GetParsedObservedForm(), /*is_username_visible=*/true,
      /*is_password_visible=*/true);
  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

TEST_P(ActorLoginCredentialFillerTest, DoesntFillIfReauthFails) {
  const url::Origin origin = url::Origin::Create(GURL(kLoginUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
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
  base::test::TestFuture<LoginStatusResultOrError> future;
  ON_CALL(mock_is_task_in_focus_, Run).WillByDefault(Return(true));
  auto filler = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission(), &mock_client_,
      mqls_logger(), mock_is_task_in_focus_.Get(), future.GetCallback());
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));

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

  filler->AttemptLogin(&mock_password_manager_);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorDeviceReauthFailed);
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_REAUTH_FAILED);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      *parsed_form, /*is_username_visible=*/true, /*is_password_visible=*/true);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));
  // Destroy the filler, because it sends logs in the destructor.
  filler.reset();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorLoginCredentialFillerTest,
                         ::testing::Bool());

}  // namespace actor_login
