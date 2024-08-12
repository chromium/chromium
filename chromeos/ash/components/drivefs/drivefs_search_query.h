// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_QUERY_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_QUERY_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace drivefs {

class DriveFsSearchQueryDelegate;

// A single search query to DriveFS.
// Destroy this class to stop any searches.
//
// Currently only used by `DriveFsSearch`.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsSearchQuery {
 public:
  DriveFsSearchQuery(base::WeakPtr<DriveFsSearchQueryDelegate> delegate,
                     mojom::QueryParametersPtr query);
  DriveFsSearchQuery(const DriveFsSearchQuery&) = delete;
  DriveFsSearchQuery& operator=(const DriveFsSearchQuery&) = delete;
  ~DriveFsSearchQuery();

  mojom::QueryParameters::QuerySource source();

  // THIS SHOULD NOT BE CALLED MORE THAN ONCE.
  // TODO: b/357980197 - Fix pagination for non-local results.
  //
  // The supplied callback may not be called if the remote is not bound, or the
  // remote is disconnected.
  // Use
  //     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
  //         callback, drive::FILE_ERROR_ABORT, std::nullopt)
  // to ensure it is always called.
  void GetNextPage(mojom::SearchQuery::GetNextPageCallback callback);

 private:
  // Initializes `remote_` based on `query_` and offline status. Can be called
  // after the constructor to re-initialize `remote_`.
  void Init();

  void OnGetNextPage(
      mojom::SearchQuery::GetNextPageCallback callback,
      drive::FileError error,
      std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  // Adjusts `query_` for an offline search. Does not re-initialize `remote_`.
  void AdjustQueryForOffline();

  base::WeakPtr<DriveFsSearchQueryDelegate> delegate_;

  mojom::QueryParametersPtr query_;

  mojo::Remote<mojom::SearchQuery> remote_;

  base::WeakPtrFactory<DriveFsSearchQuery> weak_ptr_factory_{this};
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_QUERY_H_
