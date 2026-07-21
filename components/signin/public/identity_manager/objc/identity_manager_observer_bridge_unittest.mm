// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"

#import "base/ios/block_types.h"
#import "base/test/task_environment.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"

@interface IdentityManagerObservingFake : NSObject <IdentityManagerObserving>

@property(nonatomic, assign) NSInteger primaryAccountDidChangeCount;
@property(nonatomic, assign) NSInteger refreshTokenDidUpdateForAccountCount;
@property(nonatomic, assign) NSInteger refreshTokenWasRemovedForAccountCount;
@property(nonatomic, assign) NSInteger refreshTokensWasLoadedCount;
@property(nonatomic, assign) NSInteger accountsInCookieWasUpdatedCount;
@property(nonatomic, assign)
    NSInteger batchOfRefreshTokenStateChangesDidEndCount;
@property(nonatomic, assign) NSInteger identityManagerDidShutdownCount;
@property(nonatomic, strong) ProceduralBlock onIdentityManagerShutdownBlock;

@property(nonatomic, assign) signin::PrimaryAccountChangeEvent receivedEvent;
@property(nonatomic, assign) CoreAccountInfo receivedPrimaryAccountInfo;
@property(nonatomic, assign) CoreAccountId receivedAccountId;
@property(nonatomic, assign)
    signin::AccountsInCookieJarInfo receivedccountsInCookieJarInfo;
@property(nonatomic, assign) GoogleServiceAuthError receivedError;

@end

@implementation IdentityManagerObservingFake

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  ++self.primaryAccountDidChangeCount;
  self.receivedEvent = event;
}

- (void)refreshTokenDidUpdateForAccount:(const CoreAccountInfo&)accountInfo {
  ++self.refreshTokenDidUpdateForAccountCount;
  self.receivedPrimaryAccountInfo = accountInfo;
}

- (void)refreshTokenWasRemovedForAccount:(const CoreAccountId&)accountId {
  ++self.refreshTokenWasRemovedForAccountCount;
  self.receivedAccountId = accountId;
}

- (void)refreshTokensWasLoaded {
  ++self.refreshTokensWasLoadedCount;
}

- (void)accountsInCookieWasUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                             error:(const GoogleServiceAuthError&)error {
  ++self.accountsInCookieWasUpdatedCount;
  self.receivedccountsInCookieJarInfo = accountsInCookieJarInfo;
  self.receivedError = error;
}

- (void)batchOfRefreshTokenStateChangesDidEnd {
  ++self.batchOfRefreshTokenStateChangesDidEndCount;
}

- (void)identityManagerDidShutdown:(signin::IdentityManager*)identityManager {
  ++self.identityManagerDidShutdownCount;
  if (self.onIdentityManagerShutdownBlock) {
    self.onIdentityManagerShutdownBlock();
  }
}

@end

namespace signin {

class IdentityManagerObserverBridgeTest : public testing::Test {
 protected:
  IdentityManagerObserverBridgeTest()
      : identity_test_env_(std::make_unique<signin::IdentityTestEnvironment>(
            &test_url_loader_factory_)) {
    observer_bridge_target_ = [[IdentityManagerObservingFake alloc] init];
    signin::IdentityManager* identity_manager =
        identity_test_env_->identity_manager();
    observer_bridge_ = std::make_unique<signin::IdentityManagerObserverBridge>(
        identity_manager, observer_bridge_target_);
    account_info_.gaia = GaiaId("joegaia");
    account_info_.account_id = CoreAccountId::FromGaiaId(account_info_.gaia);
    account_info_.email = "joe@example.com";
  }
  ~IdentityManagerObserverBridgeTest() override = default;

  void TearDown() override {
    // Check no unexpected calls. None zero counter needs to be reset at the end
    // tests.
    EXPECT_EQ(0, observer_bridge_target_.primaryAccountDidChangeCount);
    EXPECT_EQ(0, observer_bridge_target_.refreshTokenDidUpdateForAccountCount);
    EXPECT_EQ(0, observer_bridge_target_.refreshTokenWasRemovedForAccountCount);
    EXPECT_EQ(0, observer_bridge_target_.refreshTokensWasLoadedCount);
    EXPECT_EQ(0, observer_bridge_target_.accountsInCookieWasUpdatedCount);
    EXPECT_EQ(
        0, observer_bridge_target_.batchOfRefreshTokenStateChangesDidEndCount);
    EXPECT_EQ(0, observer_bridge_target_.identityManagerDidShutdownCount);
  }

 public:
  IdentityManagerObserverBridgeTest(const IdentityManagerObserverBridgeTest&) =
      delete;
  IdentityManagerObserverBridgeTest& operator=(
      const IdentityManagerObserverBridgeTest&) = delete;

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<signin::IdentityManagerObserverBridge> observer_bridge_;
  IdentityManagerObservingFake* observer_bridge_target_;
  CoreAccountInfo account_info_;
};

// Tests IdentityManagerObserverBridge::OnPrimaryAccountChanged(), with set
// event.
TEST_F(IdentityManagerObserverBridgeTest, TestOnPrimaryAccountChanged) {
  PrimaryAccountChangeEvent::State previous_state;
  PrimaryAccountChangeEvent::State current_state(account_info_,
                                                 signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::AccessPoint::kStartPage);
  observer_bridge_.get()->OnPrimaryAccountChanged(event_details);
  EXPECT_EQ(1, observer_bridge_target_.primaryAccountDidChangeCount);
  EXPECT_EQ(event_details.GetPreviousState(),
            observer_bridge_target_.receivedEvent.GetPreviousState());
  EXPECT_EQ(event_details.GetCurrentState(),
            observer_bridge_target_.receivedEvent.GetCurrentState());
  // Reset counter to pass the tear down.
  observer_bridge_target_.primaryAccountDidChangeCount = 0;
}

// Tests IdentityManagerObserverBridge::OnPrimaryAccountChanged(), with clear
// event.
TEST_F(IdentityManagerObserverBridgeTest, TestOnPrimaryAccountCleared) {
  PrimaryAccountChangeEvent::State previous_state(
      account_info_, signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent::State current_state;
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::ProfileSignout::kTest);
  observer_bridge_.get()->OnPrimaryAccountChanged(event_details);
  EXPECT_EQ(1, observer_bridge_target_.primaryAccountDidChangeCount);
  EXPECT_EQ(event_details.GetPreviousState(),
            observer_bridge_target_.receivedEvent.GetPreviousState());
  EXPECT_EQ(event_details.GetCurrentState(),
            observer_bridge_target_.receivedEvent.GetCurrentState());
  // Reset counter to pass the tear down.
  observer_bridge_target_.primaryAccountDidChangeCount = 0;
}

// Tests IdentityManagerObserverBridge::refreshTokenDidUpdateForAccount()
TEST_F(IdentityManagerObserverBridgeTest, TestrefreshTokenDidUpdateForAccount) {
  observer_bridge_.get()->OnRefreshTokenUpdatedForAccount(account_info_);
  EXPECT_EQ(1, observer_bridge_target_.refreshTokenDidUpdateForAccountCount);
  EXPECT_EQ(account_info_, observer_bridge_target_.receivedPrimaryAccountInfo);
  // Reset counter to pass the tear down.
  observer_bridge_target_.refreshTokenDidUpdateForAccountCount = 0;
}

// Tests IdentityManagerObserverBridge::OnRefreshTokenRemovedForAccount()
TEST_F(IdentityManagerObserverBridgeTest, OnRefreshTokenRemovedForAccount) {
  CoreAccountId account_id;
  observer_bridge_.get()->OnRefreshTokenRemovedForAccount(account_id);
  EXPECT_EQ(1, observer_bridge_target_.refreshTokenWasRemovedForAccountCount);
  // Reset counter to pass the tear down.
  observer_bridge_target_.refreshTokenWasRemovedForAccountCount = 0;
}

// Tests IdentityManagerObserverBridge::OnRefreshTokensLoaded()
TEST_F(IdentityManagerObserverBridgeTest, refreshTokensWasLoaded) {
  observer_bridge_.get()->OnRefreshTokensLoaded();
  EXPECT_EQ(1, observer_bridge_target_.refreshTokensWasLoadedCount);
  // Reset counter to pass the tear down.
  observer_bridge_target_.refreshTokensWasLoadedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnAccountsInCookieUpdated() with no
// error.
TEST_F(IdentityManagerObserverBridgeTest,
       OnAccountsInCookieUpdatedWithNoError) {
  gaia::ListedAccount signed_in_account;
  signed_in_account.id =
      CoreAccountId::FromGaiaId(signin::GetTestGaiaIdForEmail("1@mail.com"));
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      /*accounts_are_fresh=*/true, /*accounts=*/{signed_in_account});
  GoogleServiceAuthError noError = GoogleServiceAuthError::AuthErrorNone();
  observer_bridge_.get()->OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                                                    noError);
  EXPECT_EQ(1, observer_bridge_target_.accountsInCookieWasUpdatedCount);
  EXPECT_EQ(noError, observer_bridge_target_.receivedError);
  // Reset counter to pass the tear down.
  observer_bridge_target_.accountsInCookieWasUpdatedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnAccountsInCookieUpdated() with error.
TEST_F(IdentityManagerObserverBridgeTest, OnAccountsInCookieUpdatedWithError) {
  gaia::ListedAccount signed_out_account;
  signed_out_account.id =
      CoreAccountId::FromGaiaId(signin::GetTestGaiaIdForEmail("2@mail.com"));
  signed_out_account.signed_out = true;
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      /*accounts_are_fresh=*/false, /*accounts=*/{signed_out_account});
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED);
  observer_bridge_.get()->OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                                                    error);
  EXPECT_EQ(1, observer_bridge_target_.accountsInCookieWasUpdatedCount);
  EXPECT_EQ(error, observer_bridge_target_.receivedError);
  // Reset counter to pass the tear down.
  observer_bridge_target_.accountsInCookieWasUpdatedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnEndBatchOfRefreshTokenStateChanges().
TEST_F(IdentityManagerObserverBridgeTest,
       OnEndBatchOfRefreshTokenStateChanges) {
  observer_bridge_.get()->OnEndBatchOfRefreshTokenStateChanges();
  EXPECT_EQ(1,
            observer_bridge_target_.batchOfRefreshTokenStateChangesDidEndCount);
  // Reset counter to pass the tear down.
  observer_bridge_target_.batchOfRefreshTokenStateChangesDidEndCount = 0;
}

// Tests IdentityManagerObserverBridge::OnIdentityManagerShutdown().
TEST_F(IdentityManagerObserverBridgeTest, OnIdentityManagerShutdown) {
  EXPECT_EQ(0, observer_bridge_target_.identityManagerDidShutdownCount);

  // On shutdown, the observer needs to be stopped.
  observer_bridge_target_.onIdentityManagerShutdownBlock = ^{
    observer_bridge_.reset();
  };

  // Shut everything down.
  identity_test_env_.reset();

  // Expect to have gotten the shutdown signal.
  EXPECT_EQ(1, observer_bridge_target_.identityManagerDidShutdownCount);

  // Reset counter to pass the tear down.
  observer_bridge_target_.identityManagerDidShutdownCount = 0;
}
}  // namespace signin
