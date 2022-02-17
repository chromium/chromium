// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
//
// TODO(https://crbug.com/1260291): The `ConsistencyCookieManager` only
// listens to the `AccountReconcilor` for now, it is not updated by UI flows
// yet.
class ConsistencyCookieManager : public AccountReconcilor::Observer {
 public:
  explicit ConsistencyCookieManager(SigninClient* signin_client,
                                    AccountReconcilor* reconcilor);
  ~ConsistencyCookieManager() override;

  ConsistencyCookieManager& operator=(const ConsistencyCookieManager&) = delete;
  ConsistencyCookieManager(const ConsistencyCookieManager&) = delete;

 private:
  friend class ConsistencyCookieManagerTest;
  FRIEND_TEST_ALL_PREFIXES(ConsistencyCookieManagerTest, ReconcilorState);

  enum class CookieValue { kConsistent, kInconsistent, kUpdating };

  // Cookie name and values.
  static const char kCookieName[];
  static const char kCookieValueStringConsistent[];
  static const char kCookieValueStringInconsistent[];
  static const char kCookieValueStringUpdating[];

  // Sets the cookie to match `value`.
  static void UpdateCookie(network::mojom::CookieManager* cookie_manager,
                           CookieValue value);

  // AccountReconcilor::Observer:
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

  // Calculates the cookie value based on the reconcilor state.
  absl::optional<CookieValue> CalculateCookieValue() const;

  // Gets the new value using `CalculateCookieValue()` and sets the cookie if it
  // changed.
  void UpdateCookieIfNeeded();

  SigninClient* const signin_client_;
  AccountReconcilor* const account_reconcilor_;
  absl::optional<CookieValue> cookie_value_ = absl::nullopt;

  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      account_reconcilor_observation_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_H_
