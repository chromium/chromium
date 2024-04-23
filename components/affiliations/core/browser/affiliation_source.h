// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SOURCE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SOURCE_H_

#include <vector>

#include "base/functional/callback_forward.h"

namespace affiliations {

class FacetURI;

// The class identifies sources for which affiliations data is needed. The
// `AffiliationPrefetcher` manages the various sources and interacts with the
// `AffiliationService` to obtain the necessary affiliation data.
class AffiliationSource {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::vector<FacetURI> /* results */)>;
  virtual ~AffiliationSource() = default;

  class Observer {
   public:
    // Notifies the observer that a new facet has been added.
    virtual void OnFacetsAdded(std::vector<FacetURI> facets) = 0;
    // Notifies the observer that a facet has been removed.
    virtual void OnFacetsRemoved(std::vector<FacetURI> facets) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Requests all facets associated with the affiliations source.
  virtual void GetFacets(ResultCallback callback) = 0;

  // Requests the source to start listening for changes in its underlying data
  // layer. Updates are then relayed to the `observer`, which is the
  // AffiliationPrefetcher instance that owns the source.
  virtual void StartObserving(AffiliationSource::Observer* observer) = 0;
};

}  // namespace affiliations
#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SOURCE_H_
