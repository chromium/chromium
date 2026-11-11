// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
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
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using testing::Field;
using testing::Return;
using testing::UnorderedElementsAre;

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

  RemoteActorCredentialSharingController::CredentialsOrError FetchCredentials(
      std::string_view gaia_id,
      std::string_view domain) {
    RemoteActorCredentialSharingController controller(
        identity_test_env_.identity_manager(), &sync_service_,
        profile_store_.get(), account_store_.get());
    base::test::TestFuture<
        RemoteActorCredentialSharingController::CredentialsOrError>
        result;
    controller.Start(gaia_id, domain, result.GetCallback());
    return result.Take();
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
  AccountInfo account_info_;
};

// Verify behavior with empty Gaia ID (fails).
TEST_F(RemoteActorCredentialSharingControllerTest,
       RequestWithEmptyGaiaIdReturnsFalse) {
  EXPECT_EQ(
      FetchCredentials(/*gaia_id=*/"", /*domain=*/"google.com"),
      base::unexpected(
          RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid));
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

  RemoteActorCredentialSharingController::CredentialsOrError result =
      FetchCredentials(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                       /*domain=*/"google.com");

  // Only account_user should be returned.
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].username_value, u"account_user");
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

  RemoteActorCredentialSharingController::CredentialsOrError result =
      FetchCredentials(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                       /*domain=*/"google.com");

  // Only profile_user should be returned.
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].username_value, u"profile_user");
}

TEST_F(RemoteActorCredentialSharingControllerTest,
       RequestFailsWhenTrustedVaultKeyRequired) {
  SignIn("user@gmail.com");

  // Set trusted vault key required.
  ON_CALL(*sync_service_.GetMockUserSettings(),
          IsTrustedVaultKeyRequiredForPreferredDataTypes())
      .WillByDefault(Return(true));

  EXPECT_EQ(
      FetchCredentials(account_info_.GetGaiaId().ToString(), "google.com"),
      base::unexpected(
          RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid));
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

  RemoteActorCredentialSharingController::CredentialsOrError result =
      FetchCredentials(/*gaia_id=*/account_info_.GetGaiaId().ToString(),
                       /*domain=*/"example.com");

  // exact_user, affiliated_user, and psl_user should be returned; grouped_user
  // must be ignored.
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result,
              UnorderedElementsAre(
                  Field(&PasswordForm::username_value, u"exact_user"),
                  Field(&PasswordForm::username_value, u"affiliated_user"),
                  Field(&PasswordForm::username_value, u"psl_user")));
}

}  // namespace

}  // namespace password_manager
