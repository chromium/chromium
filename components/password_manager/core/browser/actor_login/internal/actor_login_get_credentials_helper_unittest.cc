// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_quality_logger.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor_login {

using base::test::RunUntil;
using password_manager::PasswordForm;
using password_manager::PasswordFormManager;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;
using testing::WithArg;

using GetCredentialsDetails =
    optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails;
using ParsedFormDetails =
    optimization_guide::proto::ActorLoginQuality_ParsedFormDetails;

namespace {

template <bool success>
void PostResponse(base::OnceCallback<void(bool)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

// Expect two protos to be equal if they are serialized into the same strings.
MATCHER_P(ProtoEquals, expected_message, "") {
  std::string expected_serialized, actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override, const));

  MOCK_METHOD(bool, IsFillingEnabled, (const GURL& url), (override, const));
  FakePasswordManagerClient() {
    profile_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>(
        password_manager::IsAccountStore(false));
    account_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>(
        password_manager::IsAccountStore(true));
  }
  ~FakePasswordManagerClient() override = default;

  scoped_refptr<password_manager::TestPasswordStore> profile_store() {
    return profile_store_;
  }
  scoped_refptr<password_manager::TestPasswordStore> account_store() {
    return account_store_;
  }

 private:
  // PasswordManagerClient:
  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override {
    return profile_store_.get();
  }
  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override {
    return account_store_.get();
  }
  scoped_refptr<password_manager::TestPasswordStore> profile_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_store_;
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(const url::Origin&,
              GetLastCommittedOrigin,
              (),
              (const, override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsDirectChildOfPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsNestedWithinFencedFrame, (), (const, override));
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override));
  MOCK_METHOD(void,
              CheckViewAreaVisible,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
              (override));
};
}  // namespace

class ActorLoginGetCredentialsHelperTest : public ::testing::Test {
 public:
  ActorLoginGetCredentialsHelperTest() = default;

  void SetUp() override {
    client_.profile_store()->Init(/*affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(/*affiliated_match_helper=*/nullptr);
    ON_CALL(password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&form_cache_));
    ON_CALL(password_manager_, GetClient()).WillByDefault(Return(&client_));
    ON_CALL(client_, GetPasswordManager)
        .WillByDefault(Return(&password_manager_));
    ON_CALL(client_, IsFillingEnabled).WillByDefault(Return(true));
    ON_CALL(driver_, CheckViewAreaVisible)
        .WillByDefault(WithArg<1>(&PostResponse<true>));
  }

  void TearDown() override {
    client_.profile_store()->ShutdownOnUIThread();
    client_.account_store()->ShutdownOnUIThread();
  }

 protected:
  FakePasswordManagerClient* client() { return &client_; }
  password_manager::FakeFormFetcher* form_fetcher() { return &form_fetcher_; }
  password_manager::MockPasswordManager* password_manager() {
    return &password_manager_;
  }
  NiceMock<MockPasswordManagerDriver>& driver() { return driver_; }
  NiceMock<password_manager::MockPasswordFormCache>& form_cache() {
    return form_cache_;
  }
  base::WeakPtr<MockActorLoginQualityLogger> mqls_logger() {
    return mock_mqls_logger_.AsWeakPtr();
  }

  std::unique_ptr<PasswordFormManager> CreateFormManager() {
    return CreateFormManager(kOrigin,
                             /*is_in_main_frame=*/true,
                             actor_login::CreateSigninFormData(kUrl), client(),
                             driver(), form_fetcher());
  }

  std::unique_ptr<PasswordFormManager> CreateFormManager(
      const url::Origin& origin,
      bool is_in_main_frame,
      const autofill::FormData& form_data,
      password_manager::PasswordManagerClient* client,
      MockPasswordManagerDriver& driver,
      password_manager::FakeFormFetcher* form_fetcher) {
    ON_CALL(driver, GetLastCommittedOrigin).WillByDefault(ReturnRef(origin));
    ON_CALL(driver, IsInPrimaryMainFrame)
        .WillByDefault(Return(is_in_main_frame));
    ON_CALL(driver, GetPasswordManager)
        .WillByDefault(Return(password_manager()));

    auto form_manager = std::make_unique<PasswordFormManager>(
        client, driver.AsWeakPtr(), form_data, form_fetcher,
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client),
        /*metrics_recorder=*/nullptr);
    form_manager->DisableFillingServerPredictionsForTesting();
    form_fetcher->NotifyFetchCompleted();
    return form_manager;
  }

  PasswordForm CreatePasswordForm(
      const std::string& url,
      const std::u16string& username,
      const std::u16string& password,
      PasswordForm::MatchType match_type = PasswordForm::MatchType::kExact) {
    PasswordForm form;
    form.url = GURL(url);
    form.signon_realm = form.url.spec();
    form.username_value = username;
    form.password_value = password;
    form.match_type = match_type;
    return form;
  }

  void AddFormManager(std::unique_ptr<PasswordFormManager> manager) {
    form_managers_.push_back(std::move(manager));

    ON_CALL(form_cache_, GetFormManagers)
        .WillByDefault(Return(base::span(form_managers_)));
  }

  void AdvanceClock(base::TimeDelta time) {
    task_environment_.AdvanceClock(time);
  }

  const GURL kUrl = GURL("https://foo.com");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  FakePasswordManagerClient client_;
  NiceMock<password_manager::MockPasswordManager> password_manager_;
  password_manager::FakeFormFetcher form_fetcher_;
  NiceMock<MockPasswordManagerDriver> driver_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  NiceMock<password_manager::MockPasswordFormCache> form_cache_;
  MockActorLoginQualityLogger mock_mqls_logger_;
};

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsSuccess) {
  base::test::TestFuture<CredentialsOrError> future;
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      url::Origin::Create(GURL("https://example.com")), client(),
      password_manager(), mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());

  // Check the reported logs.
  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_NO_CREDENTIALS);
  expected_details.set_getting_credentials_time_ms(0);
  EXPECT_CALL(*mqls_logger(),
              SetGetCredentialsDetails(ProtoEquals(expected_details)));
  // Destroy the helper, because it sends logs in the destructor.
  helper.reset();
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsFiltersByDomain) {
  client()->profile_store()->AddLogin(
      CreatePasswordForm("https://foo.com", u"foo_username", u"foo_password"));
  client()->account_store()->AddLogin(
      CreatePasswordForm("https://bar.com", u"bar_username", u"bar_password"));

  base::test::TestFuture<CredentialsOrError> future;
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      url::Origin::Create(GURL("https://foo.com")), client(),
      password_manager(), mqls_logger(), future.GetCallback());

  const int kRequestDurationMs = 3;
  AdvanceClock(base::Milliseconds(kRequestDurationMs));

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"foo_username");
  EXPECT_EQ(future.Get().value()[0].type, kPassword);
  EXPECT_EQ(credentials[0].source_site_or_app, u"https://foo.com/");
  EXPECT_EQ(future.Get().value()[0].request_origin,
            url::Origin::Create(GURL("https://foo.com")));
  EXPECT_FALSE(credentials[0].immediatelyAvailableToLogin);
  EXPECT_FALSE(credentials[0].has_persistent_permission);

  // Check the reported logs.
  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_NO_SIGN_IN_FORM);
  expected_details.set_permission_details(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_NO_PERMANENT_PERMISSION);
  expected_details.set_getting_credentials_time_ms(kRequestDurationMs);
  EXPECT_CALL(*mqls_logger(),
              SetGetCredentialsDetails(ProtoEquals(expected_details)));
  // Destroy the helper, because it sends logs in the destructor.
  helper.reset();
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsFromAllStores) {
  client()->profile_store()->AddLogin(
      CreatePasswordForm("https://foo.com", u"foo_username", u"foo_password"));
  client()->account_store()->AddLogin(
      CreatePasswordForm("https://foo.com", u"bar_username", u"bar_password"));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(
      url::Origin::Create(GURL("https://foo.com")), client(),
      password_manager(), mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 2u);

  std::vector<std::u16string> usernames;
  for (const auto& credential : credentials) {
    usernames.push_back(credential.username);
  }
  EXPECT_THAT(usernames,
              UnorderedElementsAre(u"foo_username", u"bar_username"));
}

TEST_F(ActorLoginGetCredentialsHelperTest, UsernameAndPasswordFieldsVisible) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kActorLoginFieldVisibilityCheck);
  PasswordForm saved_form =
      CreatePasswordForm(kUrl.spec(), u"foo_username", u"foo_password");

  // Populate form_data and renderer IDs to be able to compare it with the
  // mqls logger.
  saved_form.form_data = actor_login::CreateSigninFormData(kUrl);
  saved_form.username_element_renderer_id =
      saved_form.form_data.fields()[0].renderer_id();
  saved_form.password_element_renderer_id =
      saved_form.form_data.fields()[1].renderer_id();

  // To make GetSigninFormManager return a non-nullptr value, we need to
  // populate the PasswordFormCache with a PasswordFormManager that represents
  // a sign-in form.
  AddFormManager(CreateFormManager(kOrigin, /*is_in_main_frame=*/true,
                                   saved_form.form_data, client(), driver(),
                                   form_fetcher()));
  form_fetcher()->SetBestMatches({saved_form});

  EXPECT_CALL(driver(), CheckViewAreaVisible)
      .Times(2)
      .WillRepeatedly(WithArg<1>(&PostResponse<true>));

  base::test::TestFuture<CredentialsOrError> future;
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      kOrigin, client(), password_manager(), mqls_logger(),
      future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));

  // `FakeFormFetcher::AddConsumer` implementation differs from production,
  // therefore additional manual call to NotifyFetchCompleted is needed
  // after helper above gets registered as observer of `FakeFormFetcher`.
  // Otherwise helper will never know that `FakeFormFetcher` already fetched
  // credentials and this test will crash.
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"foo_username");
  EXPECT_TRUE(credentials[0].immediatelyAvailableToLogin);
  EXPECT_FALSE(credentials[0].has_persistent_permission);

  // Check the reported logs.
  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_SIGN_IN_FORM_EXISTS);
  expected_details.set_permission_details(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_NO_PERMANENT_PERMISSION);
  expected_details.set_getting_credentials_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      saved_form, /*is_username_visible=*/true, /*is_password_visible=*/true);

  EXPECT_CALL(*mqls_logger(),
              SetGetCredentialsDetails(ProtoEquals(expected_details)));
  // Destroy the helper, because it sends logs in the destructor.
  helper.reset();
}

TEST_F(ActorLoginGetCredentialsHelperTest, FieldsAreNotVisible) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kActorLoginFieldVisibilityCheck);
  PasswordForm saved_form =
      CreatePasswordForm(kUrl.spec(), u"foo_username", u"foo_password");
  saved_form.actor_login_approved = true;

  saved_form.form_data = actor_login::CreateSigninFormData(kUrl);
  // Populate form_data and renderer IDs to be able to compare it with the
  // mqls logger.
  saved_form.username_element_renderer_id =
      saved_form.form_data.fields()[0].renderer_id();
  saved_form.password_element_renderer_id =
      saved_form.form_data.fields()[1].renderer_id();

  // There won't be a signin form, so the credential will be fetched from
  // the store rather than the fake form fetcher.
  client()->profile_store()->AddLogin(saved_form);

  // To make GetSigninFormManager return a non-nullptr value, we need to
  // populate the PasswordFormCache with a PasswordFormManager that
  // represents a sign-in form.
  AddFormManager(CreateFormManager(kOrigin, /*is_in_main_frame=*/true,
                                   saved_form.form_data, client(), driver(),
                                   form_fetcher()));
  form_fetcher()->SetBestMatches({saved_form});

  EXPECT_CALL(driver(), CheckViewAreaVisible)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(false));

  base::test::TestFuture<CredentialsOrError> future;
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      kOrigin, client(), password_manager(), mqls_logger(),
      future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));

  // `FakeFormFetcher::AddConsumer` implementation differs from production,
  // therefore additional manual call to NotifyFetchCompleted is needed
  // after helper above gets registered as observer of `FakeFormFetcher`.
  // Otherwise helper will never know that `FakeFormFetcher` already fetched
  // credentials and this test will crash.
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"foo_username");
  EXPECT_FALSE(credentials[0].immediatelyAvailableToLogin);
  EXPECT_TRUE(credentials[0].has_persistent_permission);

  // Check the reported logs.
  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_NO_SIGN_IN_FORM);
  expected_details.set_permission_details(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_HAS_PERMANENT_PERMISSION);
  expected_details.set_getting_credentials_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      saved_form, /*is_username_visible=*/false, /*is_password_visible=*/false);

  EXPECT_CALL(*mqls_logger(),
              SetGetCredentialsDetails(ProtoEquals(expected_details)));
  // Destroy the helper, because it sends logs in the destructor.
  helper.reset();
}

TEST_F(ActorLoginGetCredentialsHelperTest, IgnoresFormInFencedFrame) {
  PasswordForm saved_form =
      CreatePasswordForm(kUrl.spec(), u"foo_username", u"foo_password");
  client()->profile_store()->AddLogin(saved_form);
  // To make GetSigninFormManager return a non-nullptr value, we need to
  // populate the PasswordFormCache with a PasswordFormManager that
  // represents a sign-in form.
  AddFormManager(CreateFormManager());
  form_fetcher()->SetBestMatches({saved_form});

  EXPECT_CALL(driver(), IsNestedWithinFencedFrame).WillOnce(Return(true));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());
  // `FakeFormFetcher::AddConsumer` implementation differs from production,
  // therefore additional manual call to NotifyFetchCompleted is needed
  // after helper above gets registered as observer of `FakeFormFetcher`.
  // Otherwise helper will never know that `FakeFormFetcher` already fetched
  // credentials and this test will crash.
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"foo_username");
  EXPECT_FALSE(credentials[0].immediatelyAvailableToLogin);
  EXPECT_FALSE(credentials[0].has_persistent_permission);
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       SameSiteDirectChildOfFrameFormAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginSameSiteIframeSupport);
  const GURL same_site_url = GURL("https://login.foo.com");
  const url::Origin same_site_origin = url::Origin::Create(same_site_url);
  PasswordForm saved_form =
      CreatePasswordForm(same_site_url.spec(), u"user", u"pass");
  client()->profile_store()->AddLogin(saved_form);
  AddFormManager(
      CreateFormManager(same_site_origin,
                        /*is_in_main_frame=*/false,
                        actor_login::CreateSigninFormData(same_site_url),
                        client(), driver(), form_fetcher()));
  form_fetcher()->SetBestMatches({saved_form});

  ON_CALL(driver(), IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));
  EXPECT_CALL(driver(), CheckViewAreaVisible)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(true));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));

  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_TRUE(credentials[0].immediatelyAvailableToLogin);
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       SameSiteDirectChildOfPrimaryMainFrame_FeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {password_manager::features::kActorLoginSameSiteIframeSupport});
  const GURL same_site_url = GURL("https://login.foo.com");
  const url::Origin same_site_origin = url::Origin::Create(same_site_url);
  PasswordForm saved_form =
      CreatePasswordForm(same_site_url.spec(), u"user", u"pass");
  // The same site form is ignored, so it'll end up fetching credentials
  // from the store instead of the form manager's fake form fetcher.
  client()->profile_store()->AddLogin(saved_form);
  AddFormManager(
      CreateFormManager(same_site_origin,
                        /*is_in_main_frame=*/false,
                        actor_login::CreateSigninFormData(same_site_url),
                        client(), driver(), form_fetcher()));
  form_fetcher()->SetBestMatches({saved_form});

  ON_CALL(driver(), IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(true));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());
  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_FALSE(credentials[0].immediatelyAvailableToLogin);
}

TEST_F(ActorLoginGetCredentialsHelperTest, NestedFrameWithSameOrigin) {
  base::test::ScopedFeatureList feature_list;
  const GURL same_origin_url = GURL("https://foo.com/login");
  const url::Origin same_origin = url::Origin::Create(same_origin_url);
  PasswordForm saved_form =
      CreatePasswordForm(same_origin_url.spec(), u"user", u"pass");
  AddFormManager(
      CreateFormManager(same_origin,
                        /*is_in_main_frame=*/false,
                        actor_login::CreateSigninFormData(same_origin_url),
                        client(), driver(), form_fetcher()));
  form_fetcher()->SetBestMatches({saved_form});

  ON_CALL(driver(), IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(false));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());
  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_TRUE(credentials[0].immediatelyAvailableToLogin);
}

TEST_F(ActorLoginGetCredentialsHelperTest, IgnoresSameSiteNestedFrame) {
  const GURL same_site_url = GURL("https://login.foo.com");
  const url::Origin same_site_origin = url::Origin::Create(same_site_url);

  PasswordForm saved_form =
      CreatePasswordForm(same_site_url.spec(), u"user", u"pass");

  // Populate form_data and renderer IDs so the expected proto matches the
  // actual proto (which derives data from the PasswordFormManager).
  saved_form.form_data = actor_login::CreateSigninFormData(same_site_url);
  saved_form.username_element_renderer_id =
      saved_form.form_data.fields()[0].renderer_id();
  saved_form.password_element_renderer_id =
      saved_form.form_data.fields()[1].renderer_id();

  client()->profile_store()->AddLogin(saved_form);
  AddFormManager(CreateFormManager(same_site_origin,
                                   /*is_in_main_frame=*/false,
                                   saved_form.form_data, client(), driver(),
                                   form_fetcher()));
  form_fetcher()->SetBestMatches({saved_form});

  ON_CALL(driver(), IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(false));

  base::test::TestFuture<CredentialsOrError> future;
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      kOrigin, client(), password_manager(), mqls_logger(),
      future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_FALSE(credentials[0].immediatelyAvailableToLogin);

  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_NO_SIGN_IN_FORM);
  expected_details.set_permission_details(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_NO_PERMANENT_PERMISSION);
  expected_details.set_getting_credentials_time_ms(0);

  optimization_guide::proto::ActorLoginQuality_ParsedFormDetails
      saved_form_details;
  *saved_form_details.mutable_form_data() = CreateExpectedFormData(saved_form);
  saved_form_details.set_is_valid_frame_and_origin(false);
  *expected_details.add_parsed_form_details() = saved_form_details;

  EXPECT_CALL(*mqls_logger(),
              SetGetCredentialsDetails(ProtoEquals(expected_details)));
  // Destroy the helper, because it sends logs in the destructor.
  helper.reset();
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       IgnoresSameSiteNestedFrame_FeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {password_manager::features::kActorLoginSameSiteIframeSupport});
  const GURL same_site_url = GURL("https://login.foo.com");
  const url::Origin same_site_origin = url::Origin::Create(same_site_url);
  PasswordForm saved_form =
      CreatePasswordForm(same_site_url.spec(), u"user", u"pass");
  client()->profile_store()->AddLogin(saved_form);
  AddFormManager(
      CreateFormManager(same_site_origin,
                        /*is_in_main_frame=*/false,
                        actor_login::CreateSigninFormData(same_site_url),
                        client(), driver(), form_fetcher()));
  form_fetcher()->SetBestMatches({saved_form});

  ON_CALL(driver(), IsDirectChildOfPrimaryMainFrame)
      .WillByDefault(Return(false));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());
  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_FALSE(credentials[0].immediatelyAvailableToLogin);
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsPrefersExactMatch) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  psl_match.actor_login_approved = true;
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  affiliated_match.actor_login_approved = true;
  PasswordForm exact_match =
      CreatePasswordForm(kUrl.spec(), u"exact_username", u"exact_password");
  exact_match.actor_login_approved = true;
  AddFormManager(CreateFormManager());
  form_fetcher()->SetBestMatches({exact_match, affiliated_match, psl_match});
  base::test::TestFuture<CredentialsOrError> future;

  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"exact_username");
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsPrefersAffiliatedMatch) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  psl_match.actor_login_approved = true;
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  affiliated_match.actor_login_approved = true;
  AddFormManager(CreateFormManager());
  form_fetcher()->SetBestMatches({affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());
  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));

  form_fetcher()->NotifyFetchCompleted();
  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"affiliated_username");
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsNoApprovedCredentials) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  AddFormManager(CreateFormManager());
  form_fetcher()->SetBestMatches({affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));

  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 2u);
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsIgnoresWeakApprovedCredentials) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  psl_match.actor_login_approved = true;
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  AddFormManager(CreateFormManager());
  form_fetcher()->SetBestMatches({affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));

  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 2u);
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsReturnsSingleApprovedCredential) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  affiliated_match.actor_login_approved = true;
  PasswordForm exact_match =
      CreatePasswordForm(kUrl.spec(), u"exact_username", u"exact_password");

  // Populate form_data and renderer IDs to be able to compare it with the
  // mqls logger.
  exact_match.form_data = actor_login::CreateSigninFormData(kUrl);
  exact_match.username_element_renderer_id =
      exact_match.form_data.fields()[0].renderer_id();
  exact_match.password_element_renderer_id =
      exact_match.form_data.fields()[1].renderer_id();

  AddFormManager(CreateFormManager(kOrigin, /*is_in_main_frame=*/true,
                                   exact_match.form_data, client(), driver(),
                                   form_fetcher()));
  // The order is important, as PWM would rank them in this order and we
  // still want to return the affiliated match.
  form_fetcher()->SetBestMatches({exact_match, affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      kOrigin, client(), password_manager(), mqls_logger(),
      future.GetCallback());

  // The helper only attaches itself as a consumer after all the
  // async checks for signin forms are done.
  ASSERT_TRUE(RunUntil([&]() { return form_fetcher()->HasConsumers(); }));

  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"affiliated_username");
  EXPECT_TRUE(credentials[0].has_persistent_permission);

  // Check the reported logs.
  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_SIGN_IN_FORM_EXISTS);
  expected_details.set_permission_details(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_HAS_PERMANENT_PERMISSION);
  expected_details.set_getting_credentials_time_ms(0);
  *expected_details.add_parsed_form_details() = CreateExpectedLoginFormDetails(
      exact_match, /*is_username_visible=*/true, /*is_password_visible=*/true);

  EXPECT_CALL(*mqls_logger(),
              SetGetCredentialsDetails(ProtoEquals(expected_details)));
  // Destroy the helper, because it sends logs in the destructor.
  helper.reset();
}

TEST_F(ActorLoginGetCredentialsHelperTest, FillingNotAllowed) {
  EXPECT_CALL(*client(), IsFillingEnabled(kOrigin.GetURL()))
      .WillOnce(Return(false));
  base::test::TestFuture<CredentialsOrError> future;
  GetCredentialsDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_FILLING_NOT_ALLOWED);

  expected_details.set_getting_credentials_time_ms(0);
  EXPECT_CALL(*mqls_logger(),
              SetGetCredentialsDetails(ProtoEquals(expected_details)));
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        mqls_logger(), future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kFillingNotAllowed);
}

}  // namespace actor_login
