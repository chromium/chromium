// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "net/cookies/canonical_cookie.h"

namespace network::mojom {
class CookieManager;
}

class SigninClient;

namespace signin {

// The `ConsistencyCookieManager` manages the CHROME_ID_CONSISTENCY_STATE
// cookie, which is used to display an interstitial page (a.k.a. "Mirror
// Landing") while account additions are in progress. This avoids issues where
// the user has to manually reload the page or retry their navigation after
// adding an account to the OS account manager.
// The value of the cookie depends on the state of the `AccountReconcilor` and
// whether there is a native account addition flow in progress.
// The cookie is updated only if it already exists. The cookie creation is only
// forced when a `ScopedAccountUpdate` is created, which indicates an explicit
// navigation to Gaia.
class ConsistencyCookieManager : public AccountReconcilor::Observer {
 public:
  // Sets the cookie state to "Updating" while it's alive.
  // Instances are vended by `CreateScopedAccountUpdate()` and are allowed to
  // outlive the `ConsistencyCookieManager`.
  class ScopedAccountUpdate final {
   public:
    ~ScopedAccountUpdate();

    // Move operations.
    ScopedAccountUpdate(ScopedAccountUpdate&& other);
    ScopedAccountUpdate& operator=(ScopedAccountUpdate&& other);

    // `ScopedAccountUpdate` is not copyable.
    ScopedAccountUpdate(const ScopedAccountUpdate&) = delete;
    ScopedAccountUpdate& operator=(const ScopedAccountUpdate&) = delete;

   private:
    friend ConsistencyCookieManager;
    ScopedAccountUpdate(base::WeakPtr<ConsistencyCookieManager> manager);
    base::WeakPtr<ConsistencyCookieManager> consistency_cookie_manager_;
  };

  explicit ConsistencyCookieManager(SigninClient* signin_client,
                                    AccountReconcilor* reconcilor);
  ~ConsistencyCookieManager() override;

  ConsistencyCookieManager& operator=(const ConsistencyCookieManager&) = delete;
  ConsistencyCookieManager(const ConsistencyCookieManager&) = delete;

  // Web-signin UI flows should guarantee that at least a scoped update is alive
  // for the whole flow. This starts from the user interaction and finishes when
  // the account has been added to the `IdentityManager`.
  ScopedAccountUpdate CreateScopedAccountUpdate();

  // Adds or removes an extra cookie manager where the cookie updates are
  // duplicated. It is expected that an extra cookie manager is only set
  // temporarily (e.g. for the duration of a single signin flow), with the
  // intent of importing the accounts from the main cookie manager.
  void AddExtraCookieManager(network::mojom::CookieManager* manager);
  void RemoveExtraCookieManager(network::mojom::CookieManager* manager);

  // Creates the `CanonicalCookie` corresponding to the consistency cookie.
  static std::unique_ptr<net::CanonicalCookie> CreateConsistencyCookie(
      const std::string& value);

 private:
  friend class ConsistencyCookieManagerTest;
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, MoveOperations);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, ReconcilorState);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, ScopedAccountUpdate);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest,
                           ScopedAccountUpdate_Inactive);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest,
                           UpdateAfterDestruction);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, FirstCookieUpdate);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, CookieDeleted);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, CookieInvalid);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, CookieAlreadySet);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, CoalesceCookieQueries);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, CancelPendingQuery);
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, ExtraCookieManager);

  enum class CookieValue {
    kConsistent,    // Value is "Consistent".
    kInconsistent,  // Value is "Inconsistent".
    kUpdating,      // Value is "Updating".
    kInvalid        // Any other value.
  };

  // Cookie name and values.
  static const char kCookieName[];
  static const char kCookieValueStringConsistent[];
  static const char kCookieValueStringInconsistent[];
  static const char kCookieValueStringUpdating[];

  // Returns whether `cookie` is the consistency cookie.
  static bool IsConsistencyCookie(const net::CanonicalCookie& cookie);

  // Parses the cookie value from its string representation.
  static CookieValue ParseCookieValue(const std::string& value);

  // Sets the cookie to match `value`.
  static void SetCookieValue(network::mojom::CookieManager* cookie_manager,
                             CookieValue value);

  // AccountReconcilor::Observer:
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

  // Calculates the cookie value based on the reconcilor state and the count of
  // live `ScopedAccountUpdate` instances. Returns `std::nullopt` if the value
  // cannot be computed (e.g. if the reconcilor is not started).
  std::optional<CookieValue> CalculateCookieValue() const;

  // Gets the new value using `CalculateCookieValue()` and sets the cookie if it
  // changed. If `force_creation` is false, triggers a cookie query, as it
  // should only be updated if it already exists.
  void UpdateCookieIfNeeded(bool force_creation);

  // Callback for `CookieManager::GetCookieList()`. Updates the cached value of
  // the cookie, and updates the cookie only if the cookie already exists.
  void UpdateCookieIfExists(
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& /*excluded_cookies*/);

  const raw_ptr<SigninClient> signin_client_;
  const raw_ptr<AccountReconcilor> account_reconcilor_;
  signin_metrics::AccountReconcilorState account_reconcilor_state_ =
      signin_metrics::AccountReconcilorState::kInactive;
  int scoped_update_count_ = 0;

  // Cached value of the cookie, equal to the last value that was either set or
  // queried. `std::nullopt` when the cookie is missing. Initialized as
  // `CookieValue::kInvalid` so that the first cookie update is always tried,
  // but should never be set to `CookieValue::kInvalid` after that.
  std::optional<CookieValue> cookie_value_ = CookieValue::kInvalid;

  // Pending cookie update, applied after querying the cookie value. The pending
  // update is only applied if the cookie already exists.
  std::optional<CookieValue> pending_cookie_update_;

  // Extra cookie managers where the cookie is also written. These are never
  // read, and if they go out of sync with the main cookie manager, they may
  // not be updated correctly.
  std::vector<raw_ptr<network::mojom::CookieManager, VectorExperimental>>
      extra_cookie_managers_;

  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      account_reconcilor_observation_{this};

  base::WeakPtrFactory<ConsistencyCookieManager> weak_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_H_
