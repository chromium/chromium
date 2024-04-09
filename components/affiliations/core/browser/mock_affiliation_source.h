// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_SOURCE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_SOURCE_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/affiliations/core/browser/affiliation_source.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace affiliations {

class MockAffiliationSource : public AffiliationSource {
 public:
  explicit MockAffiliationSource(AffiliationSource::Observer* observer);
  ~MockAffiliationSource() override;

  MOCK_METHOD(void, GetFacets, (ResultCallback), (override));
  MOCK_METHOD(void, StartObserving, (AffiliationSource::Observer*), (override));

  void AddFacet(FacetURI facet);
  void RemoveFacet(FacetURI facet);

 private:
  raw_ptr<AffiliationSource::Observer> observer_;
};

class MockAffiliationSourceObserver : public AffiliationSource::Observer {
 public:
  MockAffiliationSourceObserver();
  ~MockAffiliationSourceObserver() override;

  MOCK_METHOD(void, OnFacetsAdded, (std::vector<FacetURI>), (override));
  MOCK_METHOD(void, OnFacetsRemoved, (std::vector<FacetURI>), (override));
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_SOURCE_H_
