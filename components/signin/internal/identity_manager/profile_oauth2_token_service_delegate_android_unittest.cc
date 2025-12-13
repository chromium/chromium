// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_android.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/mock_profile_oauth2_token_service_observer.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrictMock;

class OAuth2TokenServiceDelegateAndroidForTest
    : public ProfileOAuth2TokenServiceDelegateAndroid {
 public:
  OAuth2TokenServiceDelegateAndroidForTest(
      AccountTrackerService* account_tracker_service)
      : ProfileOAuth2TokenServiceDelegateAndroid(account_tracker_service) {}
  void SetAccounts(const std::vector<CoreAccountId>& account_ids) {
    ProfileOAuth2TokenServiceDelegateAndroid::SetAccounts(account_ids);
  }
};

namespace signin {

namespace {

MATCHER(CoreAccountInfoEq,
        /* std::tuple<const AccountInfo&, const AccountInfo&> arg, */
        "") {
  return static_cast<const CoreAccountInfo&>(std::get<0>(arg)) ==
         static_cast<const CoreAccountInfo&>(std::get<1>(arg));
}
}  // namespace

class OAuth2TokenServiceDelegateAndroidTest : public testing::Test {
 public:
  OAuth2TokenServiceDelegateAndroidTest()
      : account_tracker_service_(CreateAccountTrackerService()) {}
  ~OAuth2TokenServiceDelegateAndroidTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());
    SetUpFakeAccountManagerFacade();
    delegate_ = std::make_unique<OAuth2TokenServiceDelegateAndroidForTest>(
        &account_tracker_service_);
    delegate_->SetOnRefreshTokenRevokedNotified(base::DoNothing());
    observer_ = std::make_unique<
        testing::StrictMock<MockProfileOAuth2TokenServiceObserver>>(
        delegate_.get());
    // Ignore uniteresting calls to `OnEndBatchChanges()`, the tests don't
    // verify them.
    EXPECT_CALL(*observer_, OnEndBatchChanges).Times(testing::AnyNumber());

    // `LoadCredentials` should happen before
    // `SeedAccountsThenReloadAllAccountsWithPrimaryAccount` is invoked.
    EXPECT_CALL(*observer_, OnRefreshTokensLoaded());
    delegate_->LoadCredentials(CoreAccountId());

    account1_ =
        CreateAccountInfo(GaiaId("gaia-id-user-1"), "user-1@example.com");
    account2_ =
        CreateAccountInfo(GaiaId("gaia-id-user-2"), "user-2@example.com");
    account3_ =
        CreateAccountInfo(GaiaId("gaia-id-user-3"), "user-3@example.com");
  }

  AccountTrackerService CreateAccountTrackerService() {
#if BUILDFLAG(IS_ANDROID)
    SetUpFakeAccountManagerFacade();
#endif
    return AccountTrackerService();
  }

  AccountInfo CreateAccountInfo(const GaiaId& gaia_id,
                                const std::string& email) {
    AccountInfo account_info =
        AccountInfo::Builder(gaia_id, email)
            .SetFullName("fullname")
            .SetGivenName("givenname")
            .SetHostedDomain("example.com")
            .SetLocale("en")
            .SetAvatarUrl("https://example.com")
            .SetAccountId(account_tracker_service_.PickAccountIdForAccount(
                gaia_id, email))
            .Build();
    AccountCapabilitiesTestMutator(&account_info.capabilities)
        .set_is_subject_to_enterprise_features(true);
    CHECK(account_info.IsValid());
    return account_info;
  }

  void SetAccounts(const std::vector<AccountInfo>& accounts) {
    account_tracker_service_.SeedAccountsInfo(
        accounts,
        /*primary_account_id=*/std::nullopt,
        /*should_remove_stale_accounts=*/false);
    std::vector<CoreAccountId> account_ids;
    for (const auto& account : accounts) {
      account_ids.push_back(account.account_id);
    }
    delegate_->SetAccounts(account_ids);
  }

  AccountTrackerService account_tracker_service_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<OAuth2TokenServiceDelegateAndroidForTest> delegate_;
  std::unique_ptr<StrictMock<MockProfileOAuth2TokenServiceObserver>> observer_;

  AccountInfo account1_;
  AccountInfo account2_;
  AccountInfo account3_;
};

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom0To0WithoutPrimaryAccount) {
  // No observer call expected.

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      /*accounts=*/{},
      /*primary_account_id=*/std::nullopt);

  EXPECT_TRUE(delegate_->GetAccounts().empty());
  EXPECT_TRUE(account_tracker_service_.GetAccounts().empty());
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom1To0AWithoutPrimaryAccount) {
  SetAccounts({account1_});
  Sequence seq;
  // Previously stored account is removed.
  EXPECT_CALL(*observer_, OnRefreshTokenRevoked(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      /*accounts=*/{},
      /*primary_account_id=*/std::nullopt);

  EXPECT_TRUE(delegate_->GetAccounts().empty());
  EXPECT_TRUE(account_tracker_service_.GetAccounts().empty());
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom1To1WithoutPrimaryAccount) {
  SetAccounts({account1_});
  Sequence seq;
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account1_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      {account1_},
      /*primary_account_id=*/std::nullopt);

  EXPECT_EQ(delegate_->GetAccounts(),
            std::vector<CoreAccountId>({account1_.account_id}));
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom1To1WithPrimaryAccount) {
  SetAccounts({account1_});
  Sequence seq;
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account1_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      {account1_}, account1_.account_id);

  EXPECT_EQ(delegate_->GetAccounts(),
            std::vector<CoreAccountId>({account1_.account_id}));
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom1To1DifferentWithPrimaryAccount) {
  SetAccounts({account1_});
  Sequence seq;
  // Primary account is available.
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account2_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);
  // Previously stored account is removed.
  EXPECT_CALL(*observer_, OnRefreshTokenRevoked(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      {account2_}, account2_.account_id);

  EXPECT_EQ(delegate_->GetAccounts(),
            std::vector<CoreAccountId>({account2_.account_id}));
  EXPECT_THAT(
      std::vector<AccountInfo>({account2_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsWhenDifferentAccountsInDelegateAndTrackerService) {
  account_tracker_service_.SeedAccountsInfo(
      {account1_},
      /*primary_account_id=*/std::nullopt,
      /*should_remove_stale_accounts=*/false);
  delegate_->SetAccounts({account2_.account_id});
  Sequence seq;
  // Primary account is available.
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account3_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account3_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);
  // Previously stored account is removed.
  EXPECT_CALL(*observer_, OnRefreshTokenRevoked(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      {account3_}, account3_.account_id);

  EXPECT_EQ(delegate_->GetAccounts(),
            std::vector<CoreAccountId>({account3_.account_id}));
  EXPECT_THAT(
      std::vector<AccountInfo>({account3_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom0To1WithPrimaryAccount) {
  Sequence seq;
  // Primary account is available.
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account1_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      {account1_}, account1_.account_id);

  EXPECT_EQ(delegate_->GetAccounts(),
            std::vector<CoreAccountId>({account1_.account_id}));
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom2To1WithPrimaryAccount) {
  SetAccounts({account1_, account2_});
  Sequence seq;
  // OnRefreshTokenAvailable fired, primary account should go first.
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account1_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);
  // Previously stored account is removed.
  EXPECT_CALL(*observer_, OnRefreshTokenRevoked(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      {account1_}, account1_.account_id);

  EXPECT_EQ(delegate_->GetAccounts(),
            std::vector<CoreAccountId>({account1_.account_id}));
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       ReloadAccountsFrom1To2WithPrimaryAccount) {
  SetAccounts({account1_});
  Sequence seq;
  // OnRefreshTokenAvailable fired, primary account should go first.
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account2_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);
  EXPECT_CALL(*observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*observer_,
              OnAuthErrorChanged(account1_.account_id,
                                 GoogleServiceAuthError::AuthErrorNone(), _))
      .InSequence(seq);

  delegate_->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      {account1_, account2_}, account2_.account_id);

  EXPECT_EQ(
      delegate_->GetAccounts(),
      std::vector<CoreAccountId>({account1_.account_id, account2_.account_id}));
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_, account2_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

}  // namespace signin
