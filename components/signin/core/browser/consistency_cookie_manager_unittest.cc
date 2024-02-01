// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/test_signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/mock_account_manager_facade.h"
#endif

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

  MOCK_METHOD4(GetCookieList,
               void(const GURL& url,
                    const net::CookieOptions& cookie_options,
                    const net::CookiePartitionKeyCollection&
                        cookie_partition_key_collection,
                    GetCookieListCallback callback));
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
#if BUILDFLAG(IS_CHROMEOS)
        &account_manager_facade_,
#endif
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
    ExpectCookieSetInManager(cookie_manager_, value);
  }

  void ExpectCookieSetInManager(MockCookieManager* manager,
                                const std::string& value) {
    const std::string expected_domain =
        std::string(".") + GaiaUrls::GetInstance()->gaia_url().host();
    EXPECT_CALL(
        *manager,
        SetCanonicalCookie(
            testing::AllOf(
                testing::Property(&net::CanonicalCookie::Name,
                                  ConsistencyCookieManager::kCookieName),
                testing::Property(&net::CanonicalCookie::Value, value),
                testing::Property(&net::CanonicalCookie::Domain,
                                  expected_domain),
                testing::Property(&net::CanonicalCookie::Path, "/"),
                testing::Property(&net::CanonicalCookie::SecureAttribute, true),
                testing::Property(&net::CanonicalCookie::IsHttpOnly, false),
                testing::Property(&net::CanonicalCookie::SameSite,
                                  net::CookieSameSite::STRICT_MODE)),
            GaiaUrls::GetInstance()->gaia_url(), testing::_, testing::_));
  }

  // Configures the default behavior of `GetCookieList()`. Passing an empty
  // value simulates a missing cookie.
  void SetCookieInManager(const std::string& value) {
    net::CookieAccessResultList cookie_list = {};
    std::unique_ptr<net::CanonicalCookie> cookie;
    if (!value.empty()) {
      cookie = ConsistencyCookieManager::CreateConsistencyCookie(value);
      cookie_list.push_back({*cookie, net::CookieAccessResult()});
    }

    ON_CALL(*cookie_manager_, GetCookieList(GaiaUrls::GetInstance()->gaia_url(),
                                            testing::_, testing::_, testing::_))
        .WillByDefault(testing::WithArg<3>(testing::Invoke(
            [cookie_list](
                network::mojom::CookieManager::GetCookieListCallback callback) {
              std::move(callback).Run(cookie_list, {});
            })));
  }

  void ExpectGetCookie() {
    EXPECT_CALL(*cookie_manager_,
                GetCookieList(GaiaUrls::GetInstance()->gaia_url(), testing::_,
                              testing::_, testing::_));
  }

  void SetReconcilorState(signin_metrics::AccountReconcilorState state) {
    account_reconcilor()->SetState(state);
  }

  AccountReconcilor* account_reconcilor() { return reconcilor_.get(); }
  MockCookieManager* cookie_manager() { return cookie_manager_; }

 private:
  TestSigninClient signin_client_{/*prefs=*/nullptr};
  raw_ptr<MockCookieManager> cookie_manager_ =
      nullptr;  // Owned by `signin_client_`.
#if BUILDFLAG(IS_CHROMEOS)
  account_manager::MockAccountManagerFacade account_manager_facade_;
#endif
  std::unique_ptr<AccountReconcilor> reconcilor_;
};

// Tests that the cookie is updated when the state of the `AccountReconcilor`
// changes.
TEST_F(ConsistencyCookieManagerTest, ReconcilorState) {
  // Ensure the cookie manager was created.
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  ASSERT_TRUE(consistency_cookie_manager);
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::AccountReconcilorState::kInactive);
  // Cookie has not been set.
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Set some initial value for the cookie.
  SetCookieInManager(ConsistencyCookieManager::kCookieValueStringConsistent);

  struct AccountReconcilorStateTestCase {
    signin_metrics::AccountReconcilorState state;
    std::optional<std::string> cookie_value;
  };

  // Iterate over all reconcilor state and check that they map to the right
  // cookie value.
  // Notes about the order:
  // - Don't start with OK, as this is the current state.
  // - Always change the reconcilor state to something that results in a
  //   different cookie value (otherwise the cookie is not updated).
  AccountReconcilorStateTestCase cases[] = {
      {signin_metrics::AccountReconcilorState::kRunning,
       ConsistencyCookieManager::kCookieValueStringUpdating},
      {signin_metrics::AccountReconcilorState::kOk,
       ConsistencyCookieManager::kCookieValueStringConsistent},
      {signin_metrics::AccountReconcilorState::kError,
       ConsistencyCookieManager::kCookieValueStringInconsistent},
      {signin_metrics::AccountReconcilorState::kScheduled,
       ConsistencyCookieManager::kCookieValueStringUpdating},
      {signin_metrics::AccountReconcilorState::kInactive, std::nullopt},
  };

  for (const AccountReconcilorStateTestCase& test_case : cases) {
    if (test_case.cookie_value.has_value()) {
      ExpectGetCookie();
      ExpectCookieSet(test_case.cookie_value.value());
    }
    SetReconcilorState(test_case.state);
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    // Update the internal state of the mock cookie manager.
    if (test_case.cookie_value.has_value())
      SetCookieInManager(test_case.cookie_value.value());
  }

  // Check that the cookie is not updated needlessly.
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::AccountReconcilorState::kInactive);
  // Set again the state that was used before INACTIVE.
  SetReconcilorState(signin_metrics::AccountReconcilorState::kScheduled);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Setting the same state again does not update the cookie.
  SetReconcilorState(signin_metrics::AccountReconcilorState::kScheduled);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Setting a state that maps to the same value does not update the cookie.
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::AccountReconcilorState::kScheduled);
  SetReconcilorState(signin_metrics::AccountReconcilorState::kRunning);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
}

// Checks that the `ScopedAccountUpdate` updates the reconcilor state and can be
// nested.
TEST_F(ConsistencyCookieManagerTest, ScopedAccountUpdate) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  // Start the reconcilor, with no cookie.
  SetCookieInManager(std::string());
  ExpectGetCookie();
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
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
    // still alive. The cookie is not queried, only the cached value is used.
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 1);

    // Destroy `update_1`. All updates are destroyed, cookie should go back to
    // "Consistent".
    SetCookieInManager(ConsistencyCookieManager::kCookieValueStringUpdating);
    ExpectGetCookie();
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
            signin_metrics::AccountReconcilorState::kInactive);
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
    SetCookieInManager(
        ConsistencyCookieManager::kCookieValueStringInconsistent);
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
    testing::Mock::VerifyAndClearExpectations(cookie_manager());

    // Destroy `update`. This resets the state to "Consistent".
    SetCookieInManager(ConsistencyCookieManager::kCookieValueStringUpdating);
    ExpectGetCookie();
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  }

  testing::Mock::VerifyAndClearExpectations(cookie_manager());
}

// Tests the move operator and constructor of `ScopedAccountUpdate`.
TEST_F(ConsistencyCookieManagerTest, MoveOperations) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  // Start the reconcilor, with no cookie.
  SetCookieInManager(std::string());
  ExpectGetCookie();
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
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
  SetCookieInManager(ConsistencyCookieManager::kCookieValueStringUpdating);
  ExpectGetCookie();
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  update_ptr.reset();
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  EXPECT_EQ(consistency_cookie_manager->scoped_update_count_, 0);
}

// `ScopedAccountUpdate` can safely outlive the `AccountReconcilor`.
TEST_F(ConsistencyCookieManagerTest, UpdateAfterDestruction) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  // Start the reconcilor, with no cookie.
  SetCookieInManager(std::string());
  ExpectGetCookie();
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
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

// Tests that the deleted cookie is only re-created when a new
// `ScopedAccountUpdate` is created.
TEST_F(ConsistencyCookieManagerTest, CookieDeleted) {
  ConsistencyCookieManager* consistency_cookie_manager =
      GetConsistencyCookieManager();
  // No cookie.
  SetCookieInManager(std::string());
  // Start the reconcilor, the cookie is not created.
  ExpectGetCookie();
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());

  // Create a cookie with "Updating" value, cookie creation is forced.
  {
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    ConsistencyCookieManager::ScopedAccountUpdate update =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    // Simulate cookie deletion.
    SetCookieInManager(std::string());
    // Destroy the `ScopedAccountUpdate`, this will try to set the cookie to
    // "Consistent". However, since the cookie does not exist, the operation is
    // aborted and the cookie is not set.
    ExpectGetCookie();
  }

  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Creating a new `ScopedAccountUpdate` re-creates the cookie.
  {
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    ConsistencyCookieManager::ScopedAccountUpdate update =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    ExpectGetCookie();
  }
}

// Tests that an invalid value of the cookie is overridden.
TEST_F(ConsistencyCookieManagerTest, CookieInvalid) {
  // Set invalid cookie.
  SetCookieInManager("invalid_value");
  ExpectGetCookie();
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringConsistent);
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
}

// Tests that the cookie is not set if it already has the desired value.
TEST_F(ConsistencyCookieManagerTest, CookieAlreadySet) {
  // Set cookie to "Consistent".
  SetCookieInManager(ConsistencyCookieManager::kCookieValueStringConsistent);
  // Start the reconcilor. This queries the cookie, but does not set it again.
  ExpectGetCookie();
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
}

// Check that concurrent cookie queries are coalesced and result in only one
// cookie update.
TEST_F(ConsistencyCookieManagerTest, CoalesceCookieQueries) {
  // Configure `GetCookieList()` to be hanging.
  network::mojom::CookieManager::GetCookieListCallback get_cookie_callback;
  EXPECT_CALL(*cookie_manager(),
              GetCookieList(GaiaUrls::GetInstance()->gaia_url(), testing::_,
                            testing::_, testing::_))
      .WillOnce(testing::WithArg<3>(testing::Invoke(
          [&get_cookie_callback](
              network::mojom::CookieManager::GetCookieListCallback callback) {
            get_cookie_callback = std::move(callback);
          })));

  // Perform multiple account reconcilor changes, while the cookie query is in
  // progress. `GetCookieList()` is called only once.
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
  SetReconcilorState(signin_metrics::AccountReconcilorState::kRunning);
  SetReconcilorState(signin_metrics::AccountReconcilorState::kError);
  // Unblock `GetCookieList()`. `SetCanonicalCookie()` is called only once, with
  // the most recent value.
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringInconsistent);
  std::unique_ptr<net::CanonicalCookie> cookie =
      ConsistencyCookieManager::CreateConsistencyCookie("dummy");
  net::CookieAccessResultList cookie_list = {
      {*cookie, net::CookieAccessResult()}};
  std::move(get_cookie_callback).Run(cookie_list, {});
}

// A forced cookie update cancels the pending query.
TEST_F(ConsistencyCookieManagerTest, CancelPendingQuery) {
  // Configure `GetCookieList()` to be hanging.
  network::mojom::CookieManager::GetCookieListCallback get_cookie_callback;
  EXPECT_CALL(*cookie_manager(),
              GetCookieList(GaiaUrls::GetInstance()->gaia_url(), testing::_,
                            testing::_, testing::_))
      .WillOnce(testing::WithArg<3>(testing::Invoke(
          [&get_cookie_callback](
              network::mojom::CookieManager::GetCookieListCallback callback) {
            get_cookie_callback = std::move(callback);
          })));
  // Start a cookie query.
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);
  // While the query is in progress, trigger a forced update, the cookie is set
  // immediately, and the query is canceled.
  {
    ConsistencyCookieManager* consistency_cookie_manager =
        GetConsistencyCookieManager();
    ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
    ConsistencyCookieManager::ScopedAccountUpdate update =
        consistency_cookie_manager->CreateScopedAccountUpdate();
    // Let the query complete, nothing happens. The cookie value is ignored, as
    // it is most likely outdated.
    std::unique_ptr<net::CanonicalCookie> cookie =
        ConsistencyCookieManager::CreateConsistencyCookie(
            ConsistencyCookieManager::kCookieValueStringConsistent);
    net::CookieAccessResultList cookie_list = {
        {*cookie, net::CookieAccessResult()}};
    std::move(get_cookie_callback).Run(cookie_list, {});
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
    // Creating a nested `ScopedAccountUpdate` does not trigger anything, even
    // though the query returned "Consistent", because that value was not set in
    // the cache.
    {
      ConsistencyCookieManager::ScopedAccountUpdate inner_update =
          consistency_cookie_manager->CreateScopedAccountUpdate();
    }
    // Destroy the update. This triggers a new query and a new update, even
    // though the previous query returned "Consistent".
    ExpectGetCookie();
  }
}

TEST_F(ConsistencyCookieManagerTest, ExtraCookieManager) {
  // Start with "Consistent" in the main cookie manager.
  SetCookieInManager(ConsistencyCookieManager::kCookieValueStringConsistent);
  ExpectGetCookie();
  SetReconcilorState(signin_metrics::AccountReconcilorState::kOk);

  // Add an extra cookie manager, the cookie is set immediately.
  MockCookieManager extra_cookie_manager;
  ExpectCookieSetInManager(
      &extra_cookie_manager,
      ConsistencyCookieManager::kCookieValueStringConsistent);
  GetConsistencyCookieManager()->AddExtraCookieManager(&extra_cookie_manager);
  testing::Mock::VerifyAndClearExpectations(&extra_cookie_manager);

  // Cookie changes are applied in the extra manager.
  ExpectGetCookie();
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringInconsistent);
  ExpectCookieSetInManager(
      &extra_cookie_manager,
      ConsistencyCookieManager::kCookieValueStringInconsistent);
  SetReconcilorState(signin_metrics::AccountReconcilorState::kError);
  testing::Mock::VerifyAndClearExpectations(&extra_cookie_manager);

  // Changes from the `ScopedAccountUpdate` are applied too.
  ExpectGetCookie();
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringUpdating);
  ExpectCookieSetInManager(
      &extra_cookie_manager,
      ConsistencyCookieManager::kCookieValueStringUpdating);
  ConsistencyCookieManager::ScopedAccountUpdate update =
      GetConsistencyCookieManager()->CreateScopedAccountUpdate();
  testing::Mock::VerifyAndClearExpectations(&extra_cookie_manager);

  GetConsistencyCookieManager()->RemoveExtraCookieManager(
      &extra_cookie_manager);
  // Cookie is set back to inconsistent in the main manager, when the
  // `ScopedAccountUpdate` is destroyed.
  ExpectCookieSet(ConsistencyCookieManager::kCookieValueStringInconsistent);
}

}  // namespace signin
