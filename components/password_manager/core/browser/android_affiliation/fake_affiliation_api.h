// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_FAKE_AFFILIATION_API_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_FAKE_AFFILIATION_API_H_

#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/android_affiliation/fake_affiliation_fetcher.h"

namespace password_manager {

// Wraps the fake factory. Manufactures API responses to AffiliationFetcher
// requests based on a set of equivalence classes predefined by the tests.
class FakeAffiliationAPI {
 public:
  FakeAffiliationAPI();
  ~FakeAffiliationAPI();

  // Adds |affiliated_facets| to the set of equivalence classes that will form
  // the basis for calculating the fake API responses.
  void AddTestEquivalenceClass(const AffiliatedFacets& affiliated_facets);

  // Returns whether or not there is at least one pending fetch.
  bool HasPendingRequest();

  // Returns the list of facet URIs being looked up by the next pending fetch;
  // or an empty list if there are no pending fetches.
  std::vector<FacetURI> GetNextRequestedFacets();

  // Calculates the response to, and completes the next pending fetch, if any,
  // with success.
  void ServeNextRequest();

  // Completes the next pending fetch, if any, with failure.
  void FailNextRequest();

  // Ignores the next pending request, if any, without completing it.
  void IgnoreNextRequest();

  // Sets the fetcher factory through which the affiliation fetchers are
  // accessed.
  void SetFetcherFactory(FakeAffiliationFetcherFactory* fake_fetcher_factory) {
    fake_fetcher_factory_ = fake_fetcher_factory;
  }

 private:
  FakeAffiliationFetcherFactory* fake_fetcher_factory_ = nullptr;
  std::vector<AffiliatedFacets> preset_equivalence_relation_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_FAKE_AFFILIATION_API_H_
