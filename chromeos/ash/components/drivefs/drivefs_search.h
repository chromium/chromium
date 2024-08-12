// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace network {
class NetworkConnectionTracker;
}

namespace drivefs {

// Handles search queries to DriveFS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsSearch {
 public:
  DriveFsSearch(mojom::DriveFs* drivefs,
                network::NetworkConnectionTracker* network_connection_tracker,
                const base::Clock* clock);

  DriveFsSearch(const DriveFsSearch&) = delete;
  DriveFsSearch& operator=(const DriveFsSearch&) = delete;

  ~DriveFsSearch();

  // Starts DriveFs search query and returns whether it will be
  // performed localy or remotely. Assumes DriveFS to be mounted.
  mojom::QueryParameters::QuerySource PerformSearch(
      mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback);

  // Used by `DriveFsSearchQuery`.
  // TODO: b/357980197 - Move these out to a delegate interface, and consider
  // abstracting away the offline / cached query adjustment.
  bool IsOffline();

  void UpdateLastSharedWithMeResponse();
  bool WithinQueryCacheTtl();

  void StartMojoSearchQuery(mojo::PendingReceiver<mojom::SearchQuery> query,
                            mojom::QueryParametersPtr query_params);

 private:
  const raw_ptr<mojom::DriveFs, DanglingUntriaged> drivefs_;
  const raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;
  const raw_ptr<const base::Clock> clock_;
  base::Time last_shared_with_me_response_;

  base::WeakPtrFactory<DriveFsSearch> weak_ptr_factory_{this};
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
