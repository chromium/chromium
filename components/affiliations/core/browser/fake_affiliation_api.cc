// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/fake_affiliation_api.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {

FakeAffiliationAPI::FakeAffiliationAPI() = default;

FakeAffiliationAPI::~FakeAffiliationAPI() = default;

void FakeAffiliationAPI::AddTestEquivalenceClass(
    const AffiliatedFacets& affiliated_facets) {
  preset_equivalence_relation_.push_back(affiliated_facets);
}

void FakeAffiliationAPI::AddTestGrouping(const GroupedFacets& group) {
  groups_.push_back(group);
}

bool FakeAffiliationAPI::HasPendingRequest() {
  return fake_fetcher_factory_->has_pending_fetchers();
}

std::vector<FacetURI> FakeAffiliationAPI::GetNextRequestedFacets() {
  if (fake_fetcher_factory_->has_pending_fetchers())
    return fake_fetcher_factory_->PeekNextFetcher()->GetRequestedFacetURIs();
  return std::vector<FacetURI>();
}

void FakeAffiliationAPI::ServeNextRequest() {
  if (!fake_fetcher_factory_->has_pending_fetchers())
    return;

  FakeAffiliationFetcher* fetcher = fake_fetcher_factory_->PopNextFetcher();
  std::unique_ptr<AffiliationFetcherDelegate::Result> fake_response(
      new AffiliationFetcherDelegate::Result);
  for (const auto& facets : preset_equivalence_relation_) {
    bool had_intersection_with_request = base::ranges::any_of(
        fetcher->GetRequestedFacetURIs(), [&facets](const auto& uri) {
          return base::ranges::find(facets, uri, &Facet::uri) != facets.end();
        });
    if (had_intersection_with_request)
      fake_response->affiliations.push_back(facets);
  }
  for (const auto& group : groups_) {
    bool had_intersection_with_request = base::ranges::any_of(
        fetcher->GetRequestedFacetURIs(), [&group](const auto& uri) {
          return base::ranges::find(group.facets, uri, &Facet::uri) !=
                 group.facets.end();
        });
    if (had_intersection_with_request)
      fake_response->groupings.push_back(group);
  }
  fetcher->SimulateSuccess(std::move(fake_response));
}

void FakeAffiliationAPI::FailNextRequest() {
  if (!fake_fetcher_factory_->has_pending_fetchers())
    return;

  FakeAffiliationFetcher* fetcher = fake_fetcher_factory_->PopNextFetcher();
  fetcher->SimulateFailure();
}

void FakeAffiliationAPI::IgnoreNextRequest() {
  if (!fake_fetcher_factory_->has_pending_fetchers())
    return;
  std::ignore = fake_fetcher_factory_->PopNextFetcher();
}

}  // namespace affiliations
