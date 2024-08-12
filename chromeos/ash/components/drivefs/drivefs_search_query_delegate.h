// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_QUERY_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_QUERY_DELEGATE_H_

#include "base/component_export.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace drivefs {

// Delegate for `DriveFsSearchQuery`.
// TODO: b/357980197 - Consider abstracting away the offline / cached query
// adjustment.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
    DriveFsSearchQueryDelegate {
 public:
  // Returns whether the device is offline.
  // Used to determine whether to adjust non-local queries to be local only.
  virtual bool IsOffline() = 0;

  // Called when a cloud-only, shared-with-me, no-text, no-title query
  // successfully fetches a page.
  // Used for updating the "shared with me" cache.
  virtual void UpdateLastSharedWithMeResponse() = 0;
  // Returns whether the last "shared with me" query within the TTL is cached.
  // Used for determining whether to override the query source to be local only
  // for cloud-only, shared-with-me, no-text, no-title queries.
  virtual bool WithinQueryCacheTtl() = 0;

  // Binds `query` to a remote `mojom::SearchQuery` with the given parameters.
  // Used as a proxy for `mojom::DriveFs::StartSearchQuery`.
  virtual void StartMojoSearchQuery(
      mojo::PendingReceiver<mojom::SearchQuery> query,
      mojom::QueryParametersPtr query_params) = 0;

 protected:
  virtual ~DriveFsSearchQueryDelegate();
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_SEARCH_QUERY_DELEGATE_H_
