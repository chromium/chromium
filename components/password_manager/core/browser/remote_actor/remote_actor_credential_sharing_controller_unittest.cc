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
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/mock_sync_service.h"
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
  // RemoteActorCredentialSharingController::Delegate:
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
    profile_store_->SetAffiliatedMatchHelper(&mock_match_helper_);
    SetSyncActive(true);
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

  std::unique_ptr<RemoteActorCredentialSharingController> CreateController() {
    return std::make_unique<RemoteActorCredentialSharingController>(
        identity_test_env_.identity_manager(), &sync_service_,
        profile_store_.get(), account_store_.get(), &sharing_service_,
        &mock_client_, &delegate_);
  }

  std::unique_ptr<RemoteActorCredentialSharingController> StartController(
      std::string_view gaia_id,
      std::string_view domain,
      RemoteActorCredentialSharingController::ResultCallback callback) {
    auto controller = CreateController();
    controller->Start(gaia_id, domain, kTaskId, std::move(callback));
    return controller;
  }

  RemoteActorCredentialSharingResult RunSharingFlowAndSelect() {
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
    EXPECT_EQ(credentials.size(), 1u);
    if (credentials.size() != 1u) {
      return RemoteActorCredentialSharingResult::kOtherError;
    }

    delegate_.SelectCredential(*credentials[0]);
    return result.Get();
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
  testing::NiceMock<MockPasswordManagerClient> mock_client_;
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
       SyncInactive_LocalOnlyCredentialNotOffered) {
  SignIn("user@gmail.com");
  SetAccountStorageState(true);

  // Add password only to the local profile store (not in the account store).
  PasswordForm profile_form;
  profile_form.signon_realm = "https://google.com/";
  profile_form.url = GURL("https://google.com");
  profile_form.username_value = u"local_only_user";
  profile_form.password_value = PasswordString(u"pass");
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(profile_form));

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", result.GetCallback());

  EXPECT_EQ(result.Get(),
            RemoteActorCredentialSharingResult::kNoPasswordsFound);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       DuplicateCredentials_CollapsedToBestMatch) {
  SignIn("user@gmail.com");

  // Older exact match for "alice".
  PasswordForm older_exact_form;
  older_exact_form.signon_realm = "https://example.com/";
  older_exact_form.url = GURL("https://example.com/login");
  older_exact_form.username_element = u"old_user_field";
  older_exact_form.username_value = u"alice";
  older_exact_form.password_value = PasswordString(u"older_exact_pass");
  older_exact_form.date_last_used = base::Time::FromTimeT(100);
  older_exact_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(older_exact_form));

  // Newer exact match for "alice" (on a different form element).
  PasswordForm newer_exact_form;
  newer_exact_form.signon_realm = "https://example.com/";
  newer_exact_form.url = GURL("https://example.com/signin");
  newer_exact_form.username_element = u"new_user_field";
  newer_exact_form.username_value = u"alice";
  newer_exact_form.password_value = PasswordString(u"newer_exact_pass");
  newer_exact_form.date_last_used = base::Time::FromTimeT(200);
  newer_exact_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(newer_exact_form));

  // Even newer PSL match for "alice" (exact match still takes priority over
  // PSL).
  PasswordForm psl_form;
  psl_form.signon_realm = "https://sub.example.com/";
  psl_form.url = GURL("https://sub.example.com/login");
  psl_form.username_value = u"alice";
  psl_form.password_value = PasswordString(u"psl_pass");
  psl_form.date_last_used = base::Time::FromTimeT(300);
  psl_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(psl_form));

  // Second distinct username "bob".
  PasswordForm bob_form;
  bob_form.signon_realm = "https://example.com/";
  bob_form.url = GURL("https://example.com/login");
  bob_form.username_value = u"bob";
  bob_form.password_value = PasswordString(u"bob_pass");
  bob_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(bob_form));

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"example.com", base::DoNothing());
  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();

  // Duplicate rows for "alice" collapse to a single entry with the best match
  // (the newer exact match).
  EXPECT_THAT(credentials,
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
  SignIn("user@gmail.com");

  // Blocklisted ("Never save") entry.
  PasswordForm blocked_form;
  blocked_form.signon_realm = "https://example.com/";
  blocked_form.url = GURL("https://example.com");
  blocked_form.blocked_by_user = true;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(blocked_form));

  // Non-HTML (e.g. HTTP Basic auth) entry.
  PasswordForm basic_auth_form;
  basic_auth_form.scheme = PasswordForm::Scheme::kBasic;
  basic_auth_form.signon_realm = "https://example.com/";
  basic_auth_form.url = GURL("https://example.com");
  basic_auth_form.username_value = u"basic_user";
  basic_auth_form.password_value = PasswordString(u"basic_pass");
  basic_auth_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(basic_auth_form));

  // Federated credential entry (with non-empty password so
  // `federation_origin.IsValid()` is exercised independently).
  PasswordForm federated_form;
  federated_form.signon_realm = "federation://example.com/idp.example.com";
  federated_form.url = GURL("https://example.com");
  federated_form.username_value = u"federated_user";
  federated_form.password_value = PasswordString(u"federated_pass");
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://idp.example.com"));
  federated_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(federated_form));

  // Empty-password entry.
  PasswordForm empty_password_form;
  empty_password_form.signon_realm = "https://example.com/";
  empty_password_form.url = GURL("https://example.com");
  empty_password_form.username_value = u"empty_pass_user";
  empty_password_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(empty_password_form));

  // When only non-offer-eligible entries exist, the request fails with
  // `kNoPasswordsFound`.
  {
    base::test::TestFuture<RemoteActorCredentialSharingResult> result;
    std::unique_ptr<RemoteActorCredentialSharingController> controller =
        StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                        /*domain=*/"example.com", result.GetCallback());
    EXPECT_EQ(result.Get(),
              RemoteActorCredentialSharingResult::kNoPasswordsFound);
  }

  // When a valid password also exists alongside them, only the valid password
  // is offered.
  PasswordForm valid_form;
  valid_form.signon_realm = "https://example.com/";
  valid_form.url = GURL("https://example.com");
  valid_form.username_value = u"valid_user";
  valid_form.password_value = PasswordString(u"valid_pass");
  valid_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(valid_form));

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"example.com", base::DoNothing());
  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(Pointee(
                  Field(&PasswordForm::username_value, u"valid_user"))));
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

// Tests that a domain sent as a full origin (with the "https://" scheme) is
// handled the same way as a bare host.
TEST_F(RemoteActorCredentialSharingControllerTest,
       DomainWithHttpsScheme_IsAccepted) {
  SignIn("user@gmail.com");

  PasswordForm exact_form;
  exact_form.signon_realm = "https://example.com/";
  exact_form.url = GURL("https://example.com");
  exact_form.username_value = u"exact_user";
  exact_form.password_value = PasswordString(u"pass");
  exact_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(exact_form));

  mock_match_helper_.ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://example.com/",
                         GURL("https://example.com/")),
      /*affiliated_realms=*/{}, /*grouped_realms=*/{});

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"https://example.com", base::DoNothing());

  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(Pointee(
                  Field(&PasswordForm::username_value, u"exact_user"))));
}

// Tests that a full HTTP URL with user info, mixed case, a non-default port
// and a path is canonicalized to its origin, keeping the sender's scheme,
// before querying the stores.
TEST_F(RemoteActorCredentialSharingControllerTest,
       DomainWithFullHttpUrl_IsCanonicalizedToOrigin) {
  SignIn("user@gmail.com");

  PasswordForm http_form;
  http_form.signon_realm = "http://example.com:8080/";
  http_form.url = GURL("http://example.com:8080");
  http_form.username_value = u"http_user";
  http_form.password_value = PasswordString(u"pass");
  http_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(http_form));

  mock_match_helper_.ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(PasswordForm::Scheme::kHtml,
                         "http://example.com:8080/",
                         GURL("http://example.com:8080/")),
      /*affiliated_realms=*/{}, /*grouped_realms=*/{});

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"http://User:pw@Example.COM:8080/login?x=1",
                      base::DoNothing());

  EXPECT_THAT(delegate_.TakeShownCredentials(),
              UnorderedElementsAre(
                  Pointee(Field(&PasswordForm::username_value, u"http_user"))));
}

// Tests that a domain that does not form a valid origin is rejected with
// `kOtherError` instead of querying the stores with an opaque origin.
TEST_F(RemoteActorCredentialSharingControllerTest,
       DomainWithInvalidOrigin_IsRejected) {
  SignIn("user@gmail.com");

  constexpr std::string_view kInvalidDomains[] = {
      "", "https://", "://", "http://[invalid]", "example.com:invalid_port"};
  base::HistogramTester histograms;
  for (std::string_view domain : kInvalidDomains) {
    SCOPED_TRACE(domain);
    base::test::TestFuture<RemoteActorCredentialSharingResult> result;
    std::unique_ptr<RemoteActorCredentialSharingController> controller =
        StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                        domain, result.GetCallback());

    EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kOtherError);
  }
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kOtherError,
      std::size(kInvalidDomains));
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
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          actor_oauth_client_id,
                      IsEmpty()),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          task_namespace_id,
                      Eq(std::nullopt)),
                Property(&RemoteActorCredentialSharingService::ShareParameters::
                             password_client_tag_hash,
                         Not(IsEmpty()))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  delegate_.SelectCredential(*credentials[0]);

  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kSuccess);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       SuccessFlow_ForwardsConfiguredAgentParameters) {
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
  auto controller = CreateController();
  controller->SetActorParameters(RemoteActorType::kSearch,
                                 "test_agent_client_id", 41);
  controller->Start(account_info_.GetGaiaId().ToString(), "google.com", kTaskId,
                    result.GetCallback());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  EXPECT_CALL(
      sharing_service_,
      SharePassword(
          AllOf(Field(&RemoteActorCredentialSharingService::ShareParameters::
                          task_id,
                      "actor_id"),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          actor_oauth_client_id,
                      "test_agent_client_id"),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          task_namespace_id,
                      Eq(41))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  delegate_.SelectCredential(*credentials[0]);
  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kSuccess);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Search.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
  histograms.ExpectTotalCount(
      "PasswordManager.RemoteActorCredentialSharing.Gemini.Result", 0);
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

class RemoteActorCredentialSharingControllerReauthTest
    : public base::test::WithFeatureOverride,
      public RemoteActorCredentialSharingControllerTest {
 public:
  RemoteActorCredentialSharingControllerReauthTest()
      : base::test::WithFeatureOverride(features::kDeviceReauthRefactor) {}

  bool is_refactor_enabled() const { return IsParamFeatureEnabled(); }
};

TEST_P(RemoteActorCredentialSharingControllerReauthTest,
       ReauthEnabled_Success) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  device_reauth::MockDeviceAuthenticator* raw_authenticator =
      authenticator.get();

  std::u16string expected_message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_IOS)
  expected_message = l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_FILLING_REAUTH, u"google.com");
#endif
  EXPECT_CALL(*raw_authenticator, AuthenticateWithMessage(expected_message, _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));

  ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  ON_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .Times(is_refactor_enabled() ? 1 : 0);
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .Times(is_refactor_enabled() ? 0 : 1);

  EXPECT_CALL(sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kSuccess);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_P(RemoteActorCredentialSharingControllerReauthTest,
       ReauthEnabled_Failure) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  device_reauth::MockDeviceAuthenticator* raw_authenticator =
      authenticator.get();

  std::u16string expected_message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_IOS)
  expected_message = l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_FILLING_REAUTH, u"google.com");
#endif
  EXPECT_CALL(*raw_authenticator, AuthenticateWithMessage(expected_message, _))
      .WillOnce(base::test::RunOnceCallback<1>(false));

  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));

  ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  ON_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .Times(is_refactor_enabled() ? 1 : 0);
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .Times(is_refactor_enabled() ? 0 : 1);

  EXPECT_CALL(sharing_service_, SharePassword).Times(0);
  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kAuthenticatorFailed);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kAuthenticatorFailed, 1);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    RemoteActorCredentialSharingControllerReauthTest);

TEST_F(RemoteActorCredentialSharingControllerTest, ReauthDisabled_NoPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDeviceReauthRefactor);

  SignIn("user@gmail.com");

  ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(false));
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequired);
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy).Times(0);
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator).Times(0);

  EXPECT_CALL(sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kSuccess);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       ReauthDisabled_NoPrompt_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kDeviceReauthRefactor);

  SignIn("user@gmail.com");

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage).Times(0);
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));

  ON_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(false));
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequired).Times(0);
  EXPECT_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy);

  EXPECT_CALL(sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kSuccess);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       ReauthRequired_NullAuthenticatorFailsClosed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDeviceReauthRefactor);
  base::HistogramTester histograms;

  SignIn("user@gmail.com");

  ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(sharing_service_, SharePassword).Times(0);

  EXPECT_EQ(RunSharingFlowAndSelect(),
            RemoteActorCredentialSharingResult::kAuthenticatorFailed);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kAuthenticatorFailed, 1);
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       DestructorCancelsOngoingAuthentication) {
  SignIn("user@gmail.com");

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  device_reauth::MockDeviceAuthenticator* raw_authenticator =
      authenticator.get();

  ON_CALL(mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  ON_CALL(mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));
  EXPECT_CALL(*raw_authenticator, AuthenticateWithMessage);
  EXPECT_CALL(*raw_authenticator, Cancel);

  std::unique_ptr<RemoteActorCredentialSharingController> controller =
      StartController(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                      /*domain=*/"google.com", base::DoNothing());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      delegate_.TakeShownCredentials();
  ASSERT_EQ(credentials.size(), 1u);

  delegate_.SelectCredential(*credentials[0]);
  controller.reset();
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
       FailureFlow_ClientUnavailable) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");

  base::test::TestFuture<RemoteActorCredentialSharingResult> result;
  auto controller = std::make_unique<RemoteActorCredentialSharingController>(
      identity_test_env_.identity_manager(), &sync_service_,
      profile_store_.get(), account_store_.get(), &sharing_service_,
      /*client=*/nullptr, &delegate_);
  controller->Start(account_info_.GetGaiaId().ToString(), "google.com", kTaskId,
                    result.GetCallback());

  EXPECT_EQ(result.Get(), RemoteActorCredentialSharingResult::kOtherError);
  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kOtherError, 1);
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
      &mock_client_, &delegate_);
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
