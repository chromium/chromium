// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/drivefs/drivefs_search_query_delegate.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace network {
class NetworkConnectionTracker;
}

namespace drivefs {

class DriveFsSearchQuery;

// Handles search queries to DriveFS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsSearch
    : public DriveFsSearchQueryDelegate {
 public:
  DriveFsSearch(mojom::DriveFs* drivefs,
                network::NetworkConnectionTracker* network_connection_tracker,
                const base::Clock* clock);

  DriveFsSearch(const DriveFsSearch&) = delete;
  DriveFsSearch& operator=(const DriveFsSearch&) = delete;

  ~DriveFsSearch() override;

  // Starts a new query, but does not call `GetNextPage`.
  // The returned `DriveFsSearchQuery` can be destructed at any time to stop any
  // in-flight `GetNextPage` calls.
  std::unique_ptr<DriveFsSearchQuery> CreateQuery(
      mojom::QueryParametersPtr query_params);

  // Starts DriveFs search query and returns whether it will be
  // performed localy or remotely. Assumes DriveFS to be mounted.
  mojom::QueryParameters::QuerySource PerformSearch(
      mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback);

  // `DriveFsSearchQueryDelegate` overrides:
  bool IsOffline() override;

  void UpdateLastSharedWithMeResponse() override;
  bool WithinQueryCacheTtl() override;

  void StartMojoSearchQuery(mojo::PendingReceiver<mojom::SearchQuery> query,
                            mojom::QueryParametersPtr query_params) override;

 private:
  const raw_ptr<mojom::DriveFs, DanglingUntriaged> drivefs_;
  const raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;
  const raw_ptr<const base::Clock> clock_;
  base::Time last_shared_with_me_response_;

  base::WeakPtrFactory<DriveFsSearch> weak_ptr_factory_{this};
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
