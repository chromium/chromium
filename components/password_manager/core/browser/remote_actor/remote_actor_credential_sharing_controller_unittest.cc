// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/device_reauth/mock_device_auth_availability_checker.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::Eq;
using testing::Field;
using testing::IsEmpty;
using testing::Not;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::UnorderedElementsAre;

constexpr char kTaskId[] = "actor_id";
constexpr std::string_view kResultHistogram =
    "PasswordManager.RemoteActorCredentialSharing.Result";

StoredCredential MakeCredential(std::string_view url_spec,
                                std::u16string_view username = u"user",
                                std::u16string_view password = u"pass") {
  StoredCredential credential;
  credential.url = GURL(url_spec);
  credential.signon_realm =
      password_manager_util::GetSignonRealm(credential.url);
  credential.username_value = std::u16string(username);
  credential.password_value = PasswordString(std::u16string(password));
  return credential;
}

std::u16string ExpectedReauthMessage(std::u16string_view domain) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_IOS)
  return l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH,
                                    std::u16string(domain));
#else
  return std::u16string();
#endif
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker>,
              GetDeviceAuthAvailabilityChecker,
              (),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthAvailabilityChecker*),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequiredLegacy,
              (device_reauth::DeviceAuthenticator*),
              (override));
};

class MockRemoteActorCredentialSharingService
    : public RemoteActorCredentialSharingService {
 public:
  MockRemoteActorCredentialSharingService() = default;
  ~MockRemoteActorCredentialSharingService() override = default;

  MOCK_METHOD(
      void,
      SharePassword,
      (const RemoteActorCredentialSharingService::ShareParameters& params,
       RemoteActorCredentialSharingService::SharePasswordCallback callback),
      (override));
};

class FakeDelegate : public RemoteActorCredentialSharingController::Delegate {
 public:
  void ShowCredentialSelection(
      std::vector<std::unique_ptr<PasswordForm>> credentials,
      const url::Origin& origin,
      base::OnceCallback<void(CredentialOrError)> callback) override {
    credential_domain_ = origin.Serialize();
    selection_callback_ = std::move(callback);
    shown_credentials_.SetValue(std::move(credentials));
  }

  std::vector<std::unique_ptr<PasswordForm>> TakeShownCredentials() {
    return shown_credentials_.Take();
  }

  const std::string& credential_domain() const { return credential_domain_; }

  void SelectCredential(CredentialOrError result) {
    ASSERT_TRUE(selection_callback_);
    std::move(selection_callback_).Run(std::move(result));
  }

 private:
  std::string credential_domain_;
  base::OnceCallback<void(CredentialOrError)> selection_callback_;
  base::test::TestFuture<std::vector<std::unique_ptr<PasswordForm>>>
      shown_credentials_;
};

class RemoteActorCredentialSharingControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_store_->Init();
    account_store_->Init();
    account_store_->SetAffiliatedMatchHelper(&mock_match_helper_);

    account_info_ = identity_test_env_.MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);

    ON_CALL(mock_client_, GetDeviceAuthAvailabilityChecker).WillByDefault([]() {
      return std::make_unique<testing::NiceMock<
          device_reauth::MockDeviceAuthAvailabilityChecker>>();
    });
    ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
        .WillByDefault(Return(false));
    ON_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
        .WillByDefault(Return(false));
  }

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
  }

  std::string gaia_id() const { return account_info_.GetGaiaId().ToString(); }

  void AddProfileLogin(std::string_view url_spec = "https://google.com",
                       std::u16string_view username = u"user",
                       std::u16string_view password = u"pass") {
    StoredCredential credential = MakeCredential(url_spec, username, password);
    credential.in_store = PasswordForm::Store::kProfileStore;
    profile_store_->AddLogin(std::move(credential));
  }

  void AddAccountLogin(StoredCredential credential) {
    credential.in_store = PasswordForm::Store::kAccountStore;
    account_store_->AddLogin(std::move(credential));
  }

  void AddAccountLogin(std::string_view url_spec = "https://google.com",
                       std::u16string_view username = u"user",
                       std::u16string_view password = u"pass") {
    AddAccountLogin(MakeCredential(url_spec, username, password));
  }

  std::unique_ptr<RemoteActorCredentialSharingController> CreateController(
      PasswordManagerClient* client,
      RemoteActorCredentialSharingService* sharing_service) {
    return std::make_unique<RemoteActorCredentialSharingController>(
        identity_test_env_.identity_manager(), &sync_service_,
        profile_store_.get(), account_store_.get(), sharing_service, client,
        &delegate_);
  }

  std::unique_ptr<RemoteActorCredentialSharingController> CreateController() {
    return CreateController(&mock_client_, &sharing_service_);
  }

  std::unique_ptr<RemoteActorCredentialSharingController> StartController(
      std::string_view domain = "google.com",
      RemoteActorCredentialSharingController::ResultCallback callback =
          base::DoNothing()) {
    auto controller = CreateController();
    controller->Start(gaia_id(), domain, kTaskId, std::move(callback));
    return controller;
  }

  RemoteActorCredentialSharingResult RunSharingFlowAndSelect() {
    AddAccountLogin();
    base::test::TestFuture<RemoteActorCredentialSharingResult> result;
    auto controller = StartController("google.com", result.GetCallback());
    std::vector<std::unique_ptr<PasswordForm>> credentials =
        delegate_.TakeShownCredentials();
    EXPECT_EQ(credentials.size(), 1u);
    if (credentials.size() != 1u) {
      return RemoteActorCredentialSharingResult::kOtherError;
    }
    delegate_.SelectCredential(*credentials[0]);
    return result.Get();
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  affiliations::FakeAffiliationService fake_affiliation_service_;
  testing::NiceMock<MockAffiliatedMatchHelper> mock_match_helper_{
      &fake_affiliation_service_};
  testing::NiceMock<MockRemoteActorCredentialSharingService> sharing_service_;
  testing::NiceMock<MockPasswordManagerClient> mock_client_;
  FakeDelegate delegate_;
  AccountInfo account_info_;
};

TEST_F(RemoteActorCredentialSharingControllerTest,
       RequestWithEmptyGaiaIdReturnsFalse) {
  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = CreateController();
  controller->Start(/*gaia_id=*/"", "google.com", kTaskId,
                    result.GetCallback());
  EXPECT_EQ(
      result.Get(),
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SyncInactive_OnlyAccountStoreQueried) {
  AddProfileLogin("https://google.com", u"profile_user");
  AddAccountLogin("https://google.com", u"account_user");

  auto controller = StartController("google.com");
  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(Pointee(
                  Field(&PasswordForm::username_value, u"account_user"))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SyncInactive_LocalOnlyCredentialNotOffered) {
  AddProfileLogin("https://google.com", u"local_only_user");

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = StartController("google.com", result.GetCallback());
  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kNoPasswordsFound);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       DuplicateCredentials_CollapsedToBestMatch) {
  StoredCredential older_exact = MakeCredential("https://example.com/login",
                                                u"alice", u"older_exact_pass");
  older_exact.username_element = u"old_user_field";
  older_exact.date_last_used = base::Time::FromTimeT(100);
  AddAccountLogin(std::move(older_exact));

  StoredCredential newer_exact = MakeCredential("https://example.com/signin",
                                                u"alice", u"newer_exact_pass");
  newer_exact.username_element = u"new_user_field";
  newer_exact.date_last_used = base::Time::FromTimeT(200);
  AddAccountLogin(std::move(newer_exact));

  // Even newer PSL match for "alice"; exact match still takes priority.
  StoredCredential psl =
      MakeCredential("https://sub.example.com/login", u"alice", u"psl_pass");
  psl.date_last_used = base::Time::FromTimeT(300);
  AddAccountLogin(std::move(psl));

  AddAccountLogin("https://example.com/login", u"bob", u"bob_pass");

  auto controller = StartController("example.com");
  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(
                  Pointee(AllOf(Field(&PasswordForm::username_value, u"alice"),
                                Field(&PasswordForm::password_value,
                                      PasswordString(u"newer_exact_pass")))),
                  Pointee(AllOf(Field(&PasswordForm::username_value, u"bob"),
                                Field(&PasswordForm::password_value,
                                      PasswordString(u"bob_pass"))))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       BlockedByUserAndNonPasswordEntries_AreNotOffered) {
  StoredCredential blocked = MakeCredential("https://example.com", u"", u"");
  blocked.blocked_by_user = true;
  AddAccountLogin(std::move(blocked));

  StoredCredential basic_auth =
      MakeCredential("https://example.com", u"basic_user", u"basic_pass");
  basic_auth.scheme = PasswordForm::Scheme::kBasic;
  AddAccountLogin(std::move(basic_auth));

  StoredCredential federated = MakeCredential(
      "https://example.com", u"federated_user", u"federated_pass");
  federated.signon_realm = "federation://example.com/idp.example.com";
  federated.federation_origin =
      url::SchemeHostPort(GURL("https://idp.example.com"));
  AddAccountLogin(std::move(federated));

  AddAccountLogin("https://example.com", u"empty_pass_user", /*password=*/u"");

  {
    base::test::TestFuture<RemoteActorCredentialSharingResult> result;
    auto controller = StartController("example.com", result.GetCallback());
    EXPECT_EQ(result.Get(),
              RemoteActorCredentialSharingResult::kNoPasswordsFound);
  }

  AddAccountLogin("https://example.com", u"valid_user", u"valid_pass");
  auto controller = StartController("example.com");
  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(Pointee(
                  Field(&PasswordForm::username_value, u"valid_user"))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SyncActive_OnlyProfileStoreQueried) {
  // Explicitly exercise the legacy sync-the-feature state where synced
  // passwords reside in the profile store.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
  AddProfileLogin("https://google.com", u"profile_user");
  AddAccountLogin("https://google.com", u"account_user");

  auto controller = StartController("google.com");
  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(Pointee(
                  Field(&PasswordForm::username_value, u"profile_user"))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       RequestFailsWhenTrustedVaultKeyRequired) {
  sync_service_.SetTrustedVaultKeyRequired(true);

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = StartController("google.com", result.GetCallback());
  EXPECT_EQ(
      result.Get(),
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       ExactAffiliatedAndPSLAllowed_GroupedIgnored) {
  AddAccountLogin("https://example.com", u"exact_user");
  AddAccountLogin("https://affiliated.com", u"affiliated_user");
  AddAccountLogin("https://www.example.com", u"psl_user");
  AddAccountLogin("https://grouped.com", u"grouped_user");

  mock_match_helper_.ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://example.com/",
                         GURL("https://example.com/")),
      /*affiliated_realms=*/{"https://affiliated.com/"},
      /*grouped_realms=*/{"https://grouped.com/"});

  auto controller = StartController("example.com");
  EXPECT_THAT(
      delegate_.TakeShownCredentials(),
      UnorderedElementsAre(
          Pointee(Field(&PasswordForm::username_value, u"exact_user")),
          Pointee(Field(&PasswordForm::username_value, u"affiliated_user")),
          Pointee(Field(&PasswordForm::username_value, u"psl_user"))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       DomainWithHttpsScheme_IsAccepted) {
  AddAccountLogin("https://example.com", u"exact_user");
  mock_match_helper_.ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://example.com/",
                         GURL("https://example.com/")),
      /*affiliated_realms=*/{}, /*grouped_realms=*/{});

  auto controller = StartController("https://example.com");
  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(Pointee(
                  Field(&PasswordForm::username_value, u"exact_user"))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       DomainWithFullHttpUrl_IsCanonicalizedToOrigin) {
  AddAccountLogin("http://example.com:8080", u"http_user");
  mock_match_helper_.ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(PasswordForm::Scheme::kHtml,
                         "http://example.com:8080/",
                         GURL("http://example.com:8080/")),
      /*affiliated_realms=*/{}, /*grouped_realms=*/{});

  auto controller =
      StartController("http://User:pw@Example.COM:8080/login?x=1");
  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(
                  Pointee(Field(&PasswordForm::username_value, u"http_user"))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       DomainWithInvalidOrigin_IsRejected) {
  constexpr std::string_view kInvalidDomains[] = {
      "", "https://", "://", "http://[invalid]", "example.com:invalid_port"};
  base::HistogramTester histograms;
  for (std::string_view domain : kInvalidDomains) {
    SCOPED_TRACE(domain);
    base::test::TestFuture<RemoteActorCredentialSharingResult> result;
    auto controller = StartController(domain, result.GetCallback());
    EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kOtherError);
  }
  histograms.ExpectUniqueSample(kResultHistogram,
                                RemoteActorCredentialSharingResult::kOtherError,
                                std::size(kInvalidDomains));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SuccessFlow_SelectCredential) {
  using ShareParams = RemoteActorCredentialSharingService::ShareParameters;
  base::HistogramTester histograms;
  AddAccountLogin();

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = StartController("google.com", result.GetCallback());
  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0]->username_value, u"user");
  EXPECT_EQ(delegate_.credential_domain(), "https://google.com");

  EXPECT_CALL(
      sharing_service_,
      SharePassword(
          AllOf(
              Field(&ShareParams::password_data,
                    Property(&sync_pb::PasswordSpecificsData::username_value,
                             "user")),
              Field(&ShareParams::password_data,
                    Property(&sync_pb::PasswordSpecificsData::password_value,
                             "pass")),
              Field(&ShareParams::web_origin, "https://google.com"),
              Field(&ShareParams::task_id, "actor_id"),
              Field(&ShareParams::actor_oauth_client_id, IsEmpty()),
              Field(&ShareParams::task_namespace_id, Eq(std::nullopt)),
              Property(&ShareParams::password_client_tag_hash, Not(IsEmpty()))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  delegate_.SelectCredential(*credentials[0]);
  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kSuccess);
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SuccessFlow_ForwardsConfiguredAgentParameters) {
  using ShareParams = RemoteActorCredentialSharingService::ShareParameters;
  base::HistogramTester histograms;
  AddAccountLogin();

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = CreateController();
  controller->SetActorParameters(RemoteActorType::kSearch,
                                 "test_agent_client_id", 41);
  controller->Start(gaia_id(), "google.com", kTaskId, result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  EXPECT_CALL(
      sharing_service_,
      SharePassword(AllOf(Field(&ShareParams::task_id, "actor_id"),
                          Field(&ShareParams::actor_oauth_client_id,
                                "test_agent_client_id"),
                          Field(&ShareParams::task_namespace_id, Eq(41))),
                    _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  delegate_.SelectCredential(*credentials[0]);
  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kSuccess);
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kSuccess, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Search.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
  histograms.ExpectTotalCount(
      "PasswordManager.RemoteActorCredentialSharing.Gemini.Result", 0);
}

TEST_F(RemoteActorCredentialSharingControllerTest, FailureFlow_SharingFailed) {
  base::HistogramTester histograms;
  EXPECT_CALL(sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(false));

  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kSharingFailed);
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kSharingFailed, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest, SuccessFlow_CancelDialog) {
  base::HistogramTester histograms;
  AddAccountLogin();

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = StartController("google.com", result.GetCallback());
  ASSERT_EQ(delegate_.TakeShownCredentials().size(), 1u);

  delegate_.SelectCredential(base::unexpected(
      RemoteActorCredentialSharingResult::kUserCancelledDialog));
  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kUserCancelledDialog);
  histograms.ExpectUniqueSample(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kUserCancelledDialog, 1);
}

class RemoteActorCredentialSharingControllerReauthTest
    : public base::test::WithFeatureOverride,
      public RemoteActorCredentialSharingControllerTest {
 public:
  RemoteActorCredentialSharingControllerReauthTest()
      : base::test::WithFeatureOverride(features::kDeviceReauthRefactor) {}

 protected:
  void ExpectReauthCheck(bool reauth_required) {
    ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
        .WillByDefault(Return(reauth_required));
    ON_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
        .WillByDefault(Return(reauth_required));
    EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequired)
        .Times(IsParamFeatureEnabled() ? 1 : 0);
    EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
        .Times(IsParamFeatureEnabled() ? 0 : 1);
  }

  void ExpectReauthPromptResult(bool auth_succeeds) {
    ExpectReauthCheck(/*reauth_required=*/true);
    auto authenticator =
        std::make_unique<device_reauth::MockDeviceAuthenticator>();
    EXPECT_CALL(*authenticator, AuthenticateWithMessage(
                                    ExpectedReauthMessage(u"google.com"), _))
        .WillOnce(base::test::RunOnceCallback<1>(auth_succeeds));
    EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
        .WillOnce(Return(ByMove(std::move(authenticator))));
  }
};

TEST_P(RemoteActorCredentialSharingControllerReauthTest,
       ReauthEnabled_Success) {
  base::HistogramTester histograms;
  ExpectReauthPromptResult(/*auth_succeeds=*/true);
  EXPECT_CALL(sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));

  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kSuccess);
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_P(RemoteActorCredentialSharingControllerReauthTest,
       ReauthEnabled_Failure) {
  base::HistogramTester histograms;
  ExpectReauthPromptResult(/*auth_succeeds=*/false);
  EXPECT_CALL(sharing_service_, SharePassword).Times(0);

  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kAuthenticatorFailed);
  histograms.ExpectUniqueSample(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kAuthenticatorFailed, 1);
}

TEST_P(RemoteActorCredentialSharingControllerReauthTest,
       ReauthDisabled_NoPrompt) {
  ExpectReauthCheck(/*reauth_required=*/false);
  if (IsParamFeatureEnabled()) {
    EXPECT_CALL(mock_client_, GetDeviceAuthenticator).Times(0);
  } else {
    auto authenticator =
        std::make_unique<device_reauth::MockDeviceAuthenticator>();
    EXPECT_CALL(*authenticator, AuthenticateWithMessage).Times(0);
    EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
        .WillOnce(Return(ByMove(std::move(authenticator))));
  }
  EXPECT_CALL(sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));

  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kSuccess);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    RemoteActorCredentialSharingControllerReauthTest);

TEST_F(RemoteActorCredentialSharingControllerTest,
       ReauthRequired_NullAuthenticatorFailsClosed) {
  base::test::ScopedFeatureList feature_list(features::kDeviceReauthRefactor);
  base::HistogramTester histograms;

  ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(sharing_service_, SharePassword).Times(0);

  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kAuthenticatorFailed);
  histograms.ExpectUniqueSample(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kAuthenticatorFailed, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       DestructorCancelsOngoingAuthentication) {
  AddAccountLogin();

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  ON_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage);
  EXPECT_CALL(*authenticator, Cancel);
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));

  auto controller = StartController("google.com");
  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  delegate_.SelectCredential(*credentials[0]);
  controller.reset();
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       FailureFlow_NoPasswordsFound) {
  base::HistogramTester histograms;
  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = StartController("google.com", result.GetCallback());

  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kNoPasswordsFound);
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kNoPasswordsFound,
      1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       FailureFlow_ClientUnavailable) {
  base::HistogramTester histograms;
  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = CreateController(/*client=*/nullptr, &sharing_service_);
  controller->Start(gaia_id(), "google.com", kTaskId, result.GetCallback());

  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kOtherError);
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kOtherError, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       FailureFlow_SharingServiceUnavailable) {
  base::HistogramTester histograms;
  AddAccountLogin();

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller =
      CreateController(&mock_client_, /*sharing_service=*/nullptr);
  controller->Start(gaia_id(), "google.com", kTaskId, result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);
  delegate_.SelectCredential(*credentials[0]);

  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kSharingServiceUnavailable);
  histograms.ExpectUniqueSample(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kSharingServiceUnavailable, 1);
}

}  // namespace

}  // namespace password_manager
