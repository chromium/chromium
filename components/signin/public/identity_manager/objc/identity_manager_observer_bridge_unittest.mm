// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"

#import "base/test/task_environment.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ObserverBridgeDelegateFake
    : NSObject <IdentityManagerObserverBridgeDelegate>

@property(nonatomic, assign) NSInteger onPrimaryAccountChangedCount;
@property(nonatomic, assign) NSInteger onRefreshTokenUpdatedForAccountCount;
@property(nonatomic, assign) NSInteger onRefreshTokenRemovedForAccountCount;
@property(nonatomic, assign) NSInteger onRefreshTokensLoadedCount;
@property(nonatomic, assign) NSInteger onAccountsInCookieUpdatedCount;
@property(nonatomic, assign)
    NSInteger onEndBatchOfRefreshTokenStateChangesCount;

@property(nonatomic, assign) signin::PrimaryAccountChangeEvent receivedEvent;
@property(nonatomic, assign) CoreAccountInfo receivedPrimaryAccountInfo;
@property(nonatomic, assign) CoreAccountId receivedAccountId;
@property(nonatomic, assign)
    signin::AccountsInCookieJarInfo receivedccountsInCookieJarInfo;
@property(nonatomic, assign) GoogleServiceAuthError receivedError;

@end

@implementation ObserverBridgeDelegateFake

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  ++self.onPrimaryAccountChangedCount;
  self.receivedEvent = event;
}

- (void)onRefreshTokenUpdatedForAccount:(const CoreAccountInfo&)accountInfo {
  ++self.onRefreshTokenUpdatedForAccountCount;
  self.receivedPrimaryAccountInfo = accountInfo;
}

- (void)onRefreshTokenRemovedForAccount:(const CoreAccountId&)accountId {
  ++self.onRefreshTokenRemovedForAccountCount;
  self.receivedAccountId = accountId;
}

- (void)onRefreshTokensLoaded {
  ++self.onRefreshTokensLoadedCount;
}

- (void)onAccountsInCookieUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                            error:(const GoogleServiceAuthError&)error {
  ++self.onAccountsInCookieUpdatedCount;
  self.receivedccountsInCookieJarInfo = accountsInCookieJarInfo;
  self.receivedError = error;
}

- (void)onEndBatchOfRefreshTokenStateChanges {
  ++self.onEndBatchOfRefreshTokenStateChangesCount;
}

@end

namespace signin {

class IdentityManagerObserverBridgeTest : public testing::Test {
 protected:
  IdentityManagerObserverBridgeTest()
      : identity_test_env_(&test_url_loader_factory_) {
    observer_bridge_delegate_ = [[ObserverBridgeDelegateFake alloc] init];
    signin::IdentityManager* identity_manager =
        identity_test_env_.identity_manager();
    observer_bridge_ = std::make_unique<signin::IdentityManagerObserverBridge>(
        identity_manager, observer_bridge_delegate_);
    account_id_ = CoreAccountId("accountid");
    account_info_.account_id = account_id_;
    account_info_.gaia = "joegaia";
    account_info_.email = "joe@example.com";

    const std::string gaia_id = signin::GetTestGaiaIdForEmail("1@mail.com");
    gaia::ListedAccount one;
    one.id = CoreAccountId(gaia_id);
    just_one_.push_back(one);
  }
  ~IdentityManagerObserverBridgeTest() override {}

  void TearDown() override {
    // Check no unexpected calls. None zero counter needs to be reset at the end
    // tests.
    EXPECT_EQ(0, observer_bridge_delegate_.onPrimaryAccountChangedCount);
    EXPECT_EQ(0,
              observer_bridge_delegate_.onRefreshTokenUpdatedForAccountCount);
    EXPECT_EQ(0,
              observer_bridge_delegate_.onRefreshTokenRemovedForAccountCount);
    EXPECT_EQ(0, observer_bridge_delegate_.onRefreshTokensLoadedCount);
    EXPECT_EQ(0, observer_bridge_delegate_.onAccountsInCookieUpdatedCount);
    EXPECT_EQ(
        0, observer_bridge_delegate_.onEndBatchOfRefreshTokenStateChangesCount);
  }

 public:
  IdentityManagerObserverBridgeTest(const IdentityManagerObserverBridgeTest&) =
      delete;
  IdentityManagerObserverBridgeTest& operator=(
      const IdentityManagerObserverBridgeTest&) = delete;

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<signin::IdentityManagerObserverBridge> observer_bridge_;
  ObserverBridgeDelegateFake* observer_bridge_delegate_;
  CoreAccountId account_id_;
  CoreAccountInfo account_info_;
  const std::vector<gaia::ListedAccount> no_account_;
  std::vector<gaia::ListedAccount> just_one_;
};

// Tests IdentityManagerObserverBridge::OnPrimaryAccountChanged(), with set
// event.
TEST_F(IdentityManagerObserverBridgeTest, TestOnPrimaryAccountSet) {
  PrimaryAccountChangeEvent::State previous_state;
  PrimaryAccountChangeEvent::State current_state(account_info_,
                                                 signin::ConsentLevel::kSync);
  PrimaryAccountChangeEvent event_details(previous_state, current_state);
  observer_bridge_.get()->OnPrimaryAccountChanged(event_details);
  EXPECT_EQ(1, observer_bridge_delegate_.onPrimaryAccountChangedCount);
  EXPECT_EQ(event_details, observer_bridge_delegate_.receivedEvent);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onPrimaryAccountChangedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnPrimaryAccountChanged(), with clear
// event.
TEST_F(IdentityManagerObserverBridgeTest, TestOnPrimaryAccountCleared) {
  PrimaryAccountChangeEvent::State previous_state(account_info_,
                                                  signin::ConsentLevel::kSync);
  PrimaryAccountChangeEvent::State current_state;
  PrimaryAccountChangeEvent event_details(previous_state, current_state);
  observer_bridge_.get()->OnPrimaryAccountChanged(event_details);
  EXPECT_EQ(1, observer_bridge_delegate_.onPrimaryAccountChangedCount);
  EXPECT_EQ(event_details, observer_bridge_delegate_.receivedEvent);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onPrimaryAccountChangedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnRefreshTokenUpdatedForAccount()
TEST_F(IdentityManagerObserverBridgeTest, TestOnRefreshTokenUpdatedForAccount) {
  observer_bridge_.get()->OnRefreshTokenUpdatedForAccount(account_info_);
  EXPECT_EQ(1, observer_bridge_delegate_.onRefreshTokenUpdatedForAccountCount);
  EXPECT_EQ(account_info_,
            observer_bridge_delegate_.receivedPrimaryAccountInfo);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onRefreshTokenUpdatedForAccountCount = 0;
}

// Tests IdentityManagerObserverBridge::OnRefreshTokenRemovedForAccount()
TEST_F(IdentityManagerObserverBridgeTest, OnRefreshTokenRemovedForAccount) {
  CoreAccountId account_id;
  observer_bridge_.get()->OnRefreshTokenRemovedForAccount(account_id);
  EXPECT_EQ(1, observer_bridge_delegate_.onRefreshTokenRemovedForAccountCount);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onRefreshTokenRemovedForAccountCount = 0;
}

// Tests IdentityManagerObserverBridge::OnRefreshTokensLoaded()
TEST_F(IdentityManagerObserverBridgeTest, OnRefreshTokensLoaded) {
  observer_bridge_.get()->OnRefreshTokensLoaded();
  EXPECT_EQ(1, observer_bridge_delegate_.onRefreshTokensLoadedCount);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onRefreshTokensLoadedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnAccountsInCookieUpdated() with no
// error.
TEST_F(IdentityManagerObserverBridgeTest,
       OnAccountsInCookieUpdatedWithNoError) {
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info = {
      true, just_one_, no_account_};
  GoogleServiceAuthError noError(GoogleServiceAuthError::State::NONE);
  observer_bridge_.get()->OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                                                    noError);
  EXPECT_EQ(1, observer_bridge_delegate_.onAccountsInCookieUpdatedCount);
  EXPECT_EQ(noError, observer_bridge_delegate_.receivedError);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onAccountsInCookieUpdatedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnAccountsInCookieUpdated() with error.
TEST_F(IdentityManagerObserverBridgeTest, OnAccountsInCookieUpdatedWithError) {
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info = {
      false, no_account_, just_one_};
  GoogleServiceAuthError error(
      GoogleServiceAuthError::State::CONNECTION_FAILED);
  observer_bridge_.get()->OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                                                    error);
  EXPECT_EQ(1, observer_bridge_delegate_.onAccountsInCookieUpdatedCount);
  EXPECT_EQ(error, observer_bridge_delegate_.receivedError);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onAccountsInCookieUpdatedCount = 0;
}

// Tests IdentityManagerObserverBridge::OnEndBatchOfRefreshTokenStateChanges().
TEST_F(IdentityManagerObserverBridgeTest,
       OnEndBatchOfRefreshTokenStateChanges) {
  observer_bridge_.get()->OnEndBatchOfRefreshTokenStateChanges();
  EXPECT_EQ(
      1, observer_bridge_delegate_.onEndBatchOfRefreshTokenStateChangesCount);
  // Reset counter to pass the tear down.
  observer_bridge_delegate_.onEndBatchOfRefreshTokenStateChangesCount = 0;
}

}
