// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_DOMAIN_MATCHING_DOMAIN_RELATION_CHECKER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_DOMAIN_MATCHING_DOMAIN_RELATION_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/affiliations/core/browser/match_type.h"
#include "url/origin.h"

class GURL;

namespace affiliations {

class AffiliationService;

// Convenience class that uses `AffiliationService` to match origins.
class DomainRelationChecker {
 public:
  explicit DomainRelationChecker(AffiliationService& affiliation_service);
  ~DomainRelationChecker();

  DomainRelationChecker(const DomainRelationChecker&) = delete;
  DomainRelationChecker& operator=(const DomainRelationChecker&) = delete;

  // Asynchronously checks the relationship between `origin_1` and `origin_2`.
  //
  // Note: This method will issue live requests to `AffiliationService`, meaning
  // the user should be mindful about QPS increase and do their own
  // calculations.
  //
  // The `result_cb` will be run with the check result or `nullopt` if there is
  // no relation.
  void Check(const url::Origin& origin_1,
             const url::Origin& origin_2,
             base::OnceCallback<void(std::optional<MatchType>)> result_cb);

  // Overload that accepts `GURL`s. They will be converted to `url::Origin`
  // internally.
  void Check(const GURL& url_1,
             const GURL& url_2,
             base::OnceCallback<void(std::optional<MatchType>)> result_cb);

 private:
  // The `AffiliationService` used to retrieve affiliations and PSL extensions.
  // It is owned by the `Profile` (as a `KeyedService`). The caller must ensure
  // that `affiliation_service_` outlives this instance.
  const raw_ref<AffiliationService> affiliation_service_;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_DOMAIN_MATCHING_DOMAIN_RELATION_CHECKER_H_
