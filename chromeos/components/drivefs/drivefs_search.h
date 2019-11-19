// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {
class NetworkConnectionTracker;
}

namespace drivefs {

// Handles search queries to DriveFS.
class COMPONENT_EXPORT(DRIVEFS) DriveFsSearch {
 public:
  DriveFsSearch(mojom::DriveFs* drivefs,
                network::NetworkConnectionTracker* network_connection_tracker,
                const base::Clock* clock);
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
      base::Optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  mojom::DriveFs* const drivefs_;
  network::NetworkConnectionTracker* const network_connection_tracker_;
  const base::Clock* const clock_;
  base::Time last_shared_with_me_response_;

  base::WeakPtrFactory<DriveFsSearch> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DriveFsSearch);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_H_
