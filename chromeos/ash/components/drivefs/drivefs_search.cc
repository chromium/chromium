// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_search.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/drivefs/drivefs_search_query.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace drivefs {

namespace {

constexpr base::TimeDelta kQueryCacheTtl = base::Minutes(5);

}  // namespace

DriveFsSearch::DriveFsSearch(
    mojom::DriveFs* drivefs,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::Clock* clock)
    : drivefs_(drivefs),
      network_connection_tracker_(network_connection_tracker),
      clock_(clock) {}

DriveFsSearch::~DriveFsSearch() = default;

std::unique_ptr<DriveFsSearchQuery> DriveFsSearch::CreateQuery(
    mojom::QueryParametersPtr query_params) {
  return std::make_unique<DriveFsSearchQuery>(weak_ptr_factory_.GetWeakPtr(),
                                              std::move(query_params));
}

mojom::QueryParameters::QuerySource DriveFsSearch::PerformSearch(
    mojom::QueryParametersPtr query,
    mojom::SearchQuery::GetNextPageCallback callback) {
  std::unique_ptr<DriveFsSearchQuery> search_query =
      CreateQuery(std::move(query));
  drivefs::mojom::QueryParameters::QuerySource source = search_query->source();

  DriveFsSearchQuery* raw_search_query = search_query.get();
  // Keep `search_query` alive until `GetNextPage` finishes running.
  raw_search_query->GetNextPage(std::move(callback).Then(base::OnceClosure(
      base::DoNothingWithBoundArgs(std::move(search_query)))));
  return source;
}

void DriveFsSearch::UpdateLastSharedWithMeResponse() {
  last_shared_with_me_response_ = clock_->Now();
}

bool DriveFsSearch::WithinQueryCacheTtl() {
  return clock_->Now() - last_shared_with_me_response_ <= kQueryCacheTtl;
}

bool DriveFsSearch::IsOffline() {
  return network_connection_tracker_->IsOffline();
}

void DriveFsSearch::StartMojoSearchQuery(
    mojo::PendingReceiver<mojom::SearchQuery> query,
    mojom::QueryParametersPtr query_params) {
  drivefs_->StartSearchQuery(std::move(query), std::move(query_params));
}

}  // namespace drivefs
