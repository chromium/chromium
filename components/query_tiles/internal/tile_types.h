// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_TYPES_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_TYPES_H_

#include "base/callback.h"

// Please keep the same order as QueryTilesRequestStatus enum in
// tools/metrics/histograms/enums.xml.
enum class TileInfoRequestStatus {
  // Initial status, request is not sent.
  kInit = 0,
  // Request completed successfully.
  kSuccess = 1,
  // Request failed. Suggesting a retry with backoff.
  kFailure = 2,
  // Request failed, suggesting a suspend.
  kShouldSuspend = 3,
  // Max value.
  kMaxValue = kShouldSuspend,
};

// Please keep the same order as QueryTilesGroupStatus enum in
// tools/metrics/histograms/enums.xml.
enum class TileGroupStatus {
  // No errors happen in tile group manager.
  kSuccess = 0,
  // Database and manager component is not fully initialized.
  kUninitialized = 1,
  // Db operations failed.
  kFailureDbOperation = 2,
  // Group data has been expired or hasn't been downloaded yet.
  kNoTiles = 3,
  // Max value.
  kMaxValue = kNoTiles,
};

// Config to control how tile images are prefetched in background task.
enum class ImagePrefetchMode {
  // No images will be prefetched.
  kNone = 0,
  // Only the top level images are prefetched.
  kTopLevel = 1,
  // All tile images are prefetched.
  kAll = 2,
  kMaxValue = kAll
};

using SuccessCallback = base::OnceCallback<void(bool)>;

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_TYPES_H_
