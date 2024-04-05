// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/mock_affiliation_source.h"

namespace affiliations {

MockAffiliationSource::MockAffiliationSource(
    AffiliationSource::Observer* observer)
    : observer_(observer) {}

void MockAffiliationSource::AddFacet(FacetURI facet) {
  observer_->OnFacetsAdded({facet});
}
void MockAffiliationSource::RemoveFacet(FacetURI facet) {
  observer_->OnFacetsRemoved({facet});
}

MockAffiliationSource::~MockAffiliationSource() = default;

MockAffiliationSourceObserver::MockAffiliationSourceObserver() = default;

MockAffiliationSourceObserver::~MockAffiliationSourceObserver() = default;

}  // namespace affiliations
