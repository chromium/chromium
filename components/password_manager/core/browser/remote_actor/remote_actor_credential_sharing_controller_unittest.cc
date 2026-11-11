// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::IsEmpty;
using testing::Not;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::UnorderedElementsAre;

constexpr char kTaskId[] = "actor_id";

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
  // RemoteActorCredentialSharingController::Delegate:
  void ShowCredentialSelection(
      std::vector<std::unique_ptr<PasswordForm>> credentials,
      const url::Origin& origin,
      base::OnceCallback<void(CredentialOrError)> callback) override {
    credential_domain_ = origin.Serialize();
    selection_callback_ = std::move(callback);
    shown_credentials_.SetValue(std::move(credentials));
  }

  void AuthenticateIfNeeded(
      const url::Origin& origin,
      base::OnceCallback<void(AuthResult)> callback) override {
    auth_origin_ = origin;
    std::move(callback).Run(auth_result_);
  }

  std::vector<std::unique_ptr<PasswordForm>> TakeShownCredentials() {
    return shown_credentials_.Take();
  }

  const std::string& credential_domain() const { return credential_domain_; }
  const url::Origin& auth_origin() const { return auth_origin_; }
  void set_auth_result(AuthResult auth_result) { auth_result_ = auth_result; }

  void SelectCredential(CredentialOrError result) {
    ASSERT_TRUE(selection_callback_);
    std::move(selection_callback_).Run(std::move(result));
  }

 private:
  std::string credential_domain_;
  url::Origin auth_origin_;
  AuthResult auth_result_ = base::ok();
  base::OnceCallback<void(CredentialOrError)> selection_callback_;
  base::test::TestFuture<std::vector<std::unique_ptr<PasswordForm>>>
      shown_credentials_;
};

class RemoteActorCredentialSharingControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_store_->Init();
    account_store_->Init();
    profile_store_->SetAffiliatedMatchHelper(&mock_match_helper_);
    SetSyncActive(true);
  }

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
  }

  void SignIn(const std::string& email) {
    account_info_ = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  void SetSyncActive(bool active) {
    ON_CALL(sync_service_, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
    ON_CALL(sync_service_, HasSyncConsent()).WillByDefault(Return(active));
    ON_CALL(*sync_service_.GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(Return(active));
    ON_CALL(*sync_service_.GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(
            Return(active ? syncer::UserSelectableTypeSet(
                                {syncer::UserSelectableType::kPasswords})
                          : syncer::UserSelectableTypeSet()));
    ON_CALL(sync_service_, GetActiveDataTypes())
        .WillByDefault(Return(active ? syncer::DataTypeSet({syncer::PASSWORDS})
                                     : syncer::DataTypeSet()));
  }

  void SetAccountStorageState(bool active) {
    ON_CALL(sync_service_, HasSyncConsent()).WillByDefault(Return(false));
    ON_CALL(*sync_service_.GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(Return(false));
    ON_CALL(sync_service_, GetTransportState())
        .WillByDefault(
            Return(active ? syncer::SyncService::TransportState::ACTIVE
                          : syncer::SyncService::TransportState::DISABLED));
    ON_CALL(*sync_service_.GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(
            Return(active ? syncer::UserSelectableTypeSet(
                                {syncer::UserSelectableType::kPasswords})
                          : syncer::UserSelectableTypeSet()));
    ON_CALL(sync_service_, GetActiveDataTypes())
        .WillByDefault(Return(active ? syncer::DataTypeSet({syncer::PASSWORDS})
                                     : syncer::DataTypeSet()));
  }

  std::unique_ptr<RemoteActorCredentialSharingController> StartController(
      std::string_view gaia_id,
      std::string_view domain,
      RemoteActorCredentialSharingController::ResultCallback callback) {
    auto controller = std::make_unique<RemoteActorCredentialSharingController>(
        identity_test_env_.identity_manager(), &sync_service_,
        profile_store_.get(), account_store_.get(), &sharing_service_,
        &delegate_);
    controller->Start(gaia_id, domain, kTaskId, std::move(callback));
    return controller;
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  testing::NiceMock<syncer::MockSyncService> sync_service_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  affiliations::FakeAffiliationService fake_affiliation_service_;
  testing::NiceMock<MockAffiliatedMatchHelper> mock_match_helper_{
      &fake_affiliation_service_};
  testing::NiceMock<MockRemoteActorCredentialSharingService> sharing_service_;
  FakeDelegate delegate_;
  AccountInfo account_info_;
};

// Verify behavior with empty Gaia ID (fails).
TEST_F(RemoteActorCredentialSharingControllerTest,
       RequestWithEmptyGaiaIdReturnsFalse) {
  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/"", /*domain=*/"google.com",
                      result.GetCallback());
  EXPECT_EQ(
      result.Get(),
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SyncInactive_OnlyAccountStoreQueried) {
  SignIn("user@gmail.com");
  SetAccountStorageState(true);

  // Add password to profile store (local-only because sync is inactive).
  PasswordForm profile_form;
  profile_form.signon_realm = "https://google.com/";
  profile_form.url = GURL("https://google.com");
  profile_form.username_value = u"profile_user";
  profile_form.password_value = PasswordString(u"pass");
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(profile_form));

  // Add password to account store.
  PasswordForm account_form;
  account_form.signon_realm = "https://google.com/";
  account_form.url = GURL("https://google.com");
  account_form.username_value = u"account_user";
  account_form.password_value = PasswordString(u"pass");
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_store_->AddLogin(FromPasswordForm(account_form));

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", base::DoNothing());
  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();

  // Only account_user should be in the picker.
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0]->username_value, u"account_user");
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SyncActive_OnlyProfileStoreQueried) {
  SignIn("user@gmail.com");
  SetSyncActive(true);

  // Add password to profile store (will be kept because sync is active).
  PasswordForm profile_form;
  profile_form.signon_realm = "https://google.com/";
  profile_form.url = GURL("https://google.com");
  profile_form.username_value = u"profile_user";
  profile_form.password_value = PasswordString(u"pass");
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(profile_form));

  // Add password to account store (should be ignored because sync feature is
  // active, which means account store is not active).
  PasswordForm account_form;
  account_form.signon_realm = "https://google.com/";
  account_form.url = GURL("https://google.com");
  account_form.username_value = u"account_user";
  account_form.password_value = PasswordString(u"pass");
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_store_->AddLogin(FromPasswordForm(account_form));

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", base::DoNothing());
  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();

  // Only profile_user should be in the picker.
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0]->username_value, u"profile_user");
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       RequestFailsWhenTrustedVaultKeyRequired) {
  SignIn("user@gmail.com");

  // Set trusted vault key required.
  ON_CALL(*sync_service_.GetMockUserSettings(),
          IsTrustedVaultKeyRequiredForPreferredDataTypes())
      .WillByDefault(Return(true));

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(account_info_.GetGaiaId().ToString(), "google.com",
                      result.GetCallback());
  EXPECT_EQ(
      result.Get(),
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       ExactAffiliatedAndPSLAllowed_GroupedIgnored) {
  SignIn("user@gmail.com");

  PasswordForm exact_form;
  exact_form.signon_realm = "https://example.com/";
  exact_form.url = GURL("https://example.com");
  exact_form.username_value = u"exact_user";
  exact_form.password_value = PasswordString(u"pass");
  exact_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(exact_form));

  PasswordForm affiliated_form;
  affiliated_form.signon_realm = "https://affiliated.com/";
  affiliated_form.url = GURL("https://affiliated.com");
  affiliated_form.username_value = u"affiliated_user";
  affiliated_form.password_value = PasswordString(u"pass");
  affiliated_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(affiliated_form));

  PasswordForm psl_form;
  psl_form.signon_realm = "https://www.example.com/";
  psl_form.url = GURL("https://www.example.com");
  psl_form.username_value = u"psl_user";
  psl_form.password_value = PasswordString(u"pass");
  psl_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(psl_form));

  PasswordForm grouped_form;
  grouped_form.signon_realm = "https://grouped.com/";
  grouped_form.url = GURL("https://grouped.com");
  grouped_form.username_value = u"grouped_user";
  grouped_form.password_value = PasswordString(u"pass");
  grouped_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(grouped_form));

  mock_match_helper_.ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://example.com/",
                         GURL("https://example.com/")),
      /*affiliated_realms=*/{"https://affiliated.com/"},
      /*grouped_realms=*/{"https://grouped.com/"});

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"example.com", base::DoNothing());
  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();

  // exact_user, affiliated_user, and psl_user should be in the picker;
  // grouped_user must be ignored.
  EXPECT_THAT(
      credentials,
      UnorderedElementsAre(
          Pointee(Field(&PasswordForm::username_value, u"exact_user")),
          Pointee(Field(&PasswordForm::username_value, u"affiliated_user")),
          Pointee(Field(&PasswordForm::username_value, u"psl_user"))));
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SuccessFlow_SelectCredential) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0]->username_value, u"user");
  EXPECT_EQ(delegate_.credential_domain(), "https://google.com");

  EXPECT_CALL(
      sharing_service_,
      SharePassword(
          AllOf(Field(&RemoteActorCredentialSharingService::ShareParameters::
                          password_data,
                      Property(&sync_pb::PasswordSpecificsData::username_value,
                               "user")),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          password_data,
                      Property(&sync_pb::PasswordSpecificsData::password_value,
                               "pass")),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          web_origin,
                      "https://google.com"),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          task_id,
                      "actor_id"),
                Property(&RemoteActorCredentialSharingService::ShareParameters::
                             password_client_tag_hash,
                         Not(IsEmpty()))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  delegate_.SelectCredential(*credentials[0]);
  EXPECT_EQ(delegate_.auth_origin(),
            url::Origin::Create(GURL("https://google.com")));

  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kSuccess);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest, FailureFlow_SharingFailed) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  EXPECT_CALL(sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(false));

  delegate_.SelectCredential(*credentials[0]);

  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kSharingFailed);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSharingFailed, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest, SuccessFlow_CancelDialog) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  delegate_.SelectCredential(base::unexpected(
      RemoteActorCredentialSharingResult::kUserCancelledDialog));

  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kUserCancelledDialog);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kUserCancelledDialog, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       FailureFlow_AuthenticationFailed) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");
  delegate_.set_auth_result(base::unexpected(
      RemoteActorCredentialSharingResult::kAuthenticatorFailed));

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  EXPECT_CALL(sharing_service_, SharePassword).Times(0);

  delegate_.SelectCredential(*credentials[0]);

  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kAuthenticatorFailed);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kAuthenticatorFailed, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       FailureFlow_NoPasswordsFound) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", result.GetCallback());

  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kNoPasswordsFound);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kNoPasswordsFound, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       FailureFlow_SharingServiceUnavailable) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = std::make_unique<RemoteActorCredentialSharingController>(
      identity_test_env_.identity_manager(), &sync_service_,
      profile_store_.get(), account_store_.get(), /*sharing_service=*/nullptr,
      &delegate_);
  controller->Start(account_info_.GetGaiaId().ToString(), "google.com", kTaskId,
                    result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  delegate_.SelectCredential(*credentials[0]);

  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kSharingServiceUnavailable);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSharingServiceUnavailable, 1);
}

}  // namespace

}  // namespace password_manager
