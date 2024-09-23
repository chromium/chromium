// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_PREFETCHER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_PREFETCHER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/affiliations/core/browser/affiliation_source.h"

namespace affiliations {

class AffiliationService;
class FacetURI;

// This class prefetches affiliation information on start-up for all registered
// affiliation sources.
class AffiliationPrefetcher : public AffiliationSource::Observer {
 public:
  explicit AffiliationPrefetcher(
      affiliations::AffiliationService* affiliation_service);
  ~AffiliationPrefetcher() override;

  // Registers an affiliation source.
  void RegisterSource(std::unique_ptr<AffiliationSource> source);

 private:
  void OnResultFromSingleSourceReceived(std::vector<FacetURI> results);

  // AffiliationSource::Observer:
  void OnFacetsAdded(std::vector<FacetURI> facets) override;
  void OnFacetsRemoved(std::vector<FacetURI> facets) override;

  // Triggered once affiliation requests for all sources are returned. It
  // collects all facets from all sources and triggers a single affiliations
  // request via the `affiliation_service_`.
  void OnResultFromAllSourcesReceived(
      std::vector<std::vector<FacetURI>> facets);

  // Initializes the prefetcher 30 seconds after start-up on two main steps:
  // 1. Make a single affiliation request for all registered sources.
  // 2. Starts observing all sources for any upcoming changes (e.g
  // added/removed facets).
  void Initialize();

  // Indicates whether passwords were fetched for all sources in `sources_`.
  bool is_ready_ = false;

  const raw_ptr<affiliations::AffiliationService> affiliation_service_ =
      nullptr;

  // Allows to aggregate all `GetFacets` results from multiple sources.
  base::RepeatingCallback<void(std::vector<FacetURI>)>
      on_facets_received_barrier_callback_;

  // Sources registered that aren't observed yet.
  std::vector<std::unique_ptr<AffiliationSource>> pending_initializations_;

  // List of affiliation sources owned and being observed by the prefetcher.
  std::vector<std::unique_ptr<AffiliationSource>> initialized_sources_;

  base::WeakPtrFactory<AffiliationPrefetcher> weak_ptr_factory_{this};
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_PREFETCHER_H_
