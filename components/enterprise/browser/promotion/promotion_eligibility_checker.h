// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_ELIGIBILITY_CHECKER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_ELIGIBILITY_CHECKER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"

namespace signin {

class IdentityManager;

}  // namespace signin

namespace enterprise_promotion {

class PromotionEligibilityChecker {
 public:
  explicit PromotionEligibilityChecker(
      signin::IdentityManager* identity_manager);

  PromotionEligibilityChecker(const PromotionEligibilityChecker&) = delete;
  PromotionEligibilityChecker& operator=(const PromotionEligibilityChecker&) =
      delete;

  ~PromotionEligibilityChecker();

  void FetchAccessToken(const CoreAccountId account_id);

  void OnAuthTokenFetched(GoogleServiceAuthError error,
                          signin::AccessTokenInfo token_info);

  std::string GetFetchedTokenForTesting();

 private:
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::string oauth_token_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::WeakPtrFactory<PromotionEligibilityChecker> weak_factory_{this};
};
}  // namespace enterprise_promotion

#endif  // COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_ELIGIBILITY_CHECKER_H_
