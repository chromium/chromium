// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_PARTITIONALLOC_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_PARTITIONALLOC_SHIMS_H_

#include "components/gwp_asan/client/export.h"

namespace gwp_asan::internal {

class GWP_ASAN_EXPORT PartitionAllocShimSupport {
 public:
  static void InstallLightweightDetectorHooks();
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_PARTITIONALLOC_SHIMS_H_
