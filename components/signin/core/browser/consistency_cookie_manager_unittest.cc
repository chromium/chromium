// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace signin {

namespace {

class MockCookieManager
    : public testing::StrictMock<network::TestCookieManager> {
 public:
  MOCK_METHOD4(SetCanonicalCookie,
               void(const net::CanonicalCookie& cookie,
                    const GURL& source_url,
                    const net::CookieOptions& cookie_options,
                    SetCanonicalCookieCallback callback));
};

}  // namespace

class ConsistencyCookieManagerTest : public testing::Test {
 public:
  ConsistencyCookieManagerTest() {
    std::unique_ptr<MockCookieManager> mock_cookie_manager =
        std::make_unique<MockCookieManager>();
    cookie_manager_ = mock_cookie_manager.get();
    signin_client_.set_cookie_manager(std::move(mock_cookie_manager));
    reconcilor_ = std::make_unique<AccountReconcilor>(
        /*identity_manager=*/nullptr, &signin_client_,
        std::make_unique<AccountReconcilorDelegate>());
  }

  ~ConsistencyCookieManagerTest() override { DeleteConsistencyCookieManager(); }

  ConsistencyCookieManager* GetConsistencyCookieManager() {
    return reconcilor_->GetConsistencyCookieManager();
  }

  void DeleteConsistencyCookieManager() {
    if (!reconcilor_)
      return;
    // `AccountReconcilor` shutdown should not trigger a cookie update.
    reconcilor_->Shutdown();
    reconcilor_.reset();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
  }

  void ExpectCookieSet(const std::string& value) {
    const std::string expected_domain =
        std::string(".") + GaiaUrls::GetInstance()->gaia_url().host();
    EXPECT_CALL(
        *cookie_manager_,
        SetCanonicalCookie(
            testing::AllOf(
                testing::Property(&net::CanonicalCookie::Name,
                                  ConsistencyCookieManager::kCookieName),
                testing::Property(&net::CanonicalCookie::Value, value),
                testing::Property(&net::CanonicalCookie::Domain,
                                  expected_domain),
                testing::Property(&net::CanonicalCookie::Path, "/"),
                testing::Property(&net::CanonicalCookie::IsSecure, true),
                testing::Property(&net::CanonicalCookie::IsHttpOnly, false)),
            GaiaUrls::GetInstance()->gaia_url(), testing::_, testing::_));
  }

  void SetReconcilorState(signin_metrics::AccountReconcilorState state) {
    account_reconcilor()->SetState(state);
  }

  AccountReconcilor* account_reconcilor() { return reconcilor_.get(); }
  MockCookieManager* cookie_manager() { return cookie_manager_; }

 private:
  // The kLacrosNonSyncingProfiles flags bundles several features: non-syncing
  // profiles, signed out profiles, and Mirror Landing. The
  // `ConsistencyCookieManager` is related to MirrorLanding.
  base::test::ScopedFeatureList feature_list_{
      switches::kLacrosNonSyncingProfiles};

  TestSigninClient signin_client_{/*prefs=*/nullptr};
  MockCookieManager* cookie_manager_ = nullptr;  // Owned by `signin_client_`.
  std::unique_ptr<AccountReconcilor> reconcilor_;
};

// Tests that the cookie is updated when the state of the `AccountReconcilor`
// changes.
TEST_F(ConsistencyCookieManagerTest, ReconcilorState) {
  // Ensure the cookie manager was created.
  ASSERT_TRUE(GetConsistencyCookieManager());
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::ACCOUNT_RECONCILOR_INACTIVE);
  // Cookie has not been set.
  testing::Mock::VerifyAndClearExpectations(cookie_manager());

  struct AccountReconcilorStateTestCase {
    signin_metrics::AccountReconcilorState state;
    absl::optional<std::string> cookie_value;
  };

  // Iterate over all reconcilor state and check that they map to the right
  // cookie value.
  // Notes about the order:
  // - Don't start with OK, as this is the current state.
  // - Always change the reconcilor state to something that results in a
  //   different cookie value (otherwise the cookie is not updated).
  AccountReconcilorStateTestCase cases[] = {
      {signin_metrics::ACCOUNT_RECONCILOR_RUNNING,
       ConsistencyCookieManager::kCookieValueStringUpdating},
      {signin_metrics::ACCOUNT_RECONCILOR_OK,
       ConsistencyCookieManager::kCookieValueStringConsistent},
      {signin_metrics::ACCOUNT_RECONCILOR_ERROR,
       ConsistencyCookieManager::kCookieValueStringInconsistent},
      {signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
       ConsistencyCookieManager::kCookieValueStringUpdating},
      {signin_metrics::ACCOUNT_RECONCILOR_INACTIVE, absl::nullopt},
  };

  for (const AccountReconcilorStateTestCase& test_case : cases) {
    if (test_case.cookie_value.has_value())
      ExpectCookieSet(test_case.cookie_value.value());
    SetReconcilorState(test_case.state);
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
  }

  // Check that the cookie is not updated needlessly.
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::ACCOUNT_RECONCILOR_INACTIVE);
  // Set again the state that was used before INACTIVE.
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Setting the same state again does not update the cookie.
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Setting a state that maps to the same value does not update the cookie.
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED);
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_RUNNING);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
}

// Checks that the `ScopedAccountUpdate` updates the reconcilor state and can be
// nested.
TEST_F(ConsistencyCookieManagerTest, ScopedAccountUpdate) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_OK);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());

  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 0);

  {
    // Create a scoped update, this sets the cookie to "Updating".
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    ConsistencyCookieManager::ScopedAccountUpdate update_1 =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    {
      // Create a second update, this does nothing, but increments the internal
      // counter. Counter is decremented when it goes out of scope.
      ConsistencyCookieManager::ScopedAccountUpdate update_2 =
          consistency_cookie_manager->CreateScopedAccountUpdate();
      testing::Mock::VerifyAndClearExpectations(cookie_manager());
      EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 2);
    }

    // `update_2` was destroyed. Cookie value did not change as `update_1` is
    // still alive.
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Destroy `update_1`. All updates are destroyed, cookue should go back to
    // "Consistent".
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  }

  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 0);
}

// Tests the behavior of `ScopedAccountUpdate` when the reconcilor is inactive.
TEST_F(ConsistencyCookieManagerTest, ScopedAccountUpdate_Inactive) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::ACCOUNT_RECONCILOR_INACTIVE);
  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 0);

  {
    // Create a scoped update, this does not change the cookie because the
    // reconcilor is inactive.
    ConsistencyCookieManager::ScopedAccountUpdate update =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Destroy `update`. Cookie is not updated, because the reconcilor is
    // inactive.
  }

  testing::Mock::VerifyAndClearExpectations(cookie_manager());

  {
    // Create a scoped update, this does not change the cookie because the
    // reconcilor is inactive.
    ConsistencyCookieManager::ScopedAccountUpdate update =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Start the reconcilor. The state is "Updating" because there is a live
    // update, even though it was created when the reconcilor was inactive.
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_OK);
    testing::Mock::VerifyAndClearExpectations(cookie_manager());

    // Destroy `update`. This resets the state to "Consistent".
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  }

  testing::Mock::VerifyAndClearExpectations(cookie_manager());
}

// Tests the move operator and constructor of `ScopedAccountUpdate`.
TEST_F(ConsistencyCookieManagerTest, MoveOperations) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_OK);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());

  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 0);

  std::unique_ptr<ConsistencyCookieManager::ScopedAccountUpdate> update_ptr;

  {
    // Create a scoped update, this sets the cookie to "Updating".
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    ConsistencyCookieManager::ScopedAccountUpdate update_1 =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Move the update on itself, this does nothing.
    // Use a `dummy` pointer as an indirection, as the compiler does not allow
    // `update_1 = std::move(update_1);`
    ConsistencyCookieManager::ScopedAccountUpdate* dummy = &update_1;
    update_1 = std::move(*dummy);
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Move the update to another instance, this does nothing.
    ConsistencyCookieManager::ScopedAccountUpdate update_2 =
        std::move(update_1);
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Move constructor works the same.
    update_ptr =
        std::make_unique<ConsistencyCookieManager::ScopedAccountUpdate>(
            std::move(update_2));
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Now delete all the updates that were moved, it does nothing.
  }

  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

  // Delete the remaining update, the cookie goes back to consistent.
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  update_ptr.reset();
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 0);
}

// `ScopedAccountUpdate` can safely outlive the `AccountReconcilor`.
TEST_F(ConsistencyCookieManagerTest, UpdateAfterDestruction) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_OK);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());

  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 0);

  {
    // Create a scoped update, this sets the cookie to "Updating".
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    ConsistencyCookieManager::ScopedAccountUpdate update_1 =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Delete the `ConsistencyCookieManager`, but not the update.
    DeleteConsistencyCookieManager();
    // The cookie is not updated.
    testing::Mock::VerifyAndClearExpectations(cookie_manager());

    // Delete the update, after the `ConsistencyCookieManager` was
    // destroyed.
  }

  // The cookie is not updated, and there is no crash.
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
}

}  // namespace signin
