// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_prefetcher.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/barrier_callback.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {
namespace {
using affiliations::FacetURI;

// I/O heavy initialization on start-up will be delayed by this long.
// This should be high enough not to exacerbate start-up I/O contention too
// much, but also low enough so important affiliations data is quickly available
// to the user (e.g. log into websites using Android credentials).
constexpr base::TimeDelta kInitializationDelayOnStartup = base::Seconds(30);
}  // namespace

AffiliationPrefetcher::AffiliationPrefetcher(
    AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AffiliationPrefetcher::Initialize,
                     weak_ptr_factory_.GetWeakPtr()),
      kInitializationDelayOnStartup);
}

AffiliationPrefetcher::~AffiliationPrefetcher() = default;

void AffiliationPrefetcher::RegisterSource(
    std::unique_ptr<AffiliationSource> source) {
  CHECK(source);
  pending_initializations_.push_back(std::move(source));

  // If initialization had already happened, request facets from all sources
  // again to ensure the affiliations cache gets properly updated, otherwise
  // do nothing.
  if (is_ready_) {
    // TODO(b/328037758): Avoid reinitializing for late registered sources and
    // directly fetch their affiliations. Include metrics to understand how
    // often does this happen.
    Initialize();
  }
}

void AffiliationPrefetcher::OnFacetsAdded(std::vector<FacetURI> facets) {
  for (const FacetURI& facet_uri : facets) {
    if (facet_uri.is_valid()) {
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
    }
  }
}

void AffiliationPrefetcher::OnFacetsRemoved(std::vector<FacetURI> facets) {
  for (const FacetURI& facet_uri : facets) {
    if (facet_uri.is_valid()) {
      affiliation_service_->CancelPrefetch(facet_uri, base::Time::Max());
    }
  }
}

void AffiliationPrefetcher::OnResultFromSingleSourceReceived(
    std::vector<FacetURI> results) {
  DCHECK(on_facets_received_barrier_callback_);
  on_facets_received_barrier_callback_.Run(std::move(results));
}

void AffiliationPrefetcher::OnResultFromAllSourcesReceived(
    std::vector<std::vector<FacetURI>> results) {
  // If any source is registered while awaiting for results from already
  // initialized sources, initialize again to account for newly added sources.
  if (!pending_initializations_.empty()) {
    Initialize();
    return;
  }

  std::vector<FacetURI> facets;
  for (const auto& result_per_source : results) {
    for (const FacetURI& facet_uri : result_per_source) {
      if (facet_uri.is_valid()) {
        facets.push_back(std::move(facet_uri));
      }
    }
  }

  affiliation_service_->KeepPrefetchForFacets(facets);
  affiliation_service_->TrimUnusedCache(std::move(facets));
  is_ready_ = true;
}

void AffiliationPrefetcher::Initialize() {
  if (pending_initializations_.empty()) {
    // Reset the cache if no sources have been registered.
    DCHECK(initialized_sources_.empty());
    affiliation_service_->KeepPrefetchForFacets({});
    affiliation_service_->TrimUnusedCache({});
    is_ready_ = true;
    return;
  }

  is_ready_ = false;
  for (std::unique_ptr<AffiliationSource>& source : pending_initializations_) {
    source->StartObserving(this);
    initialized_sources_.push_back(std::move(source));
  }

  pending_initializations_.clear();

  on_facets_received_barrier_callback_ =
      base::BarrierCallback<std::vector<FacetURI>>(
          initialized_sources_.size(),
          base::BindOnce(&AffiliationPrefetcher::OnResultFromAllSourcesReceived,
                         weak_ptr_factory_.GetWeakPtr()));

  for (const std::unique_ptr<AffiliationSource>& source :
       initialized_sources_) {
    source->GetFacets(
        base::BindOnce(&AffiliationPrefetcher::OnResultFromSingleSourceReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace affiliations
