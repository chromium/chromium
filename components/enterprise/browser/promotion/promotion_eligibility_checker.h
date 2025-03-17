// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_ELIGIBILITY_CHECKER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_ELIGIBILITY_CHECKER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"

namespace signin {

class IdentityManager;

}  // namespace signin

namespace enterprise_promotion {

class PromotionEligibilityChecker {
 public:
  using PromotionEligibilityCallback = base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>;

  explicit PromotionEligibilityChecker(
      const std::string& profile_id,
      policy::CloudPolicyClient* client,
      signin::IdentityManager* identity_manager,
      std::string locale,
      bool dismissed_banner_pref);

  PromotionEligibilityChecker(const PromotionEligibilityChecker&) = delete;
  PromotionEligibilityChecker& operator=(const PromotionEligibilityChecker&) =
      delete;

  ~PromotionEligibilityChecker();

  void MaybeCheckPromotionEligibility(
      const CoreAccountId account_id,
      PromotionEligibilityChecker::PromotionEligibilityCallback callback);

  void OnAuthTokenFetched(GoogleServiceAuthError error,
                          signin::AccessTokenInfo token_info);

  void SetCloudPolicyClientForTesting(
      std::unique_ptr<policy::CloudPolicyClient> testing_client);

  void CheckPromotionEligibilityWithAuthToken(std::string oauth_token);

 private:
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::string oauth_token_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<policy::CloudPolicyClient> promotion_query_client_;
  PromotionEligibilityCallback callback_;
  base::WeakPtrFactory<PromotionEligibilityChecker> weak_factory_{this};
};
}  // namespace enterprise_promotion

#endif  // COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_ELIGIBILITY_CHECKER_H_
