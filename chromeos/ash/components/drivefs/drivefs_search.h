// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

 private:
  void OnSearchDriveFs(
      mojo::Remote<drivefs::mojom::SearchQuery> search,
      drivefs::mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback,
      drive::FileError error,
      absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  const raw_ptr<mojom::DriveFs, ExperimentalAsh> drivefs_;
  const raw_ptr<network::NetworkConnectionTracker, ExperimentalAsh>
      network_connection_tracker_;
  const raw_ptr<const base::Clock, ExperimentalAsh> clock_;
  base::Time last_shared_with_me_response_;

  base::WeakPtrFactory<DriveFsSearch> weak_ptr_factory_{this};
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
