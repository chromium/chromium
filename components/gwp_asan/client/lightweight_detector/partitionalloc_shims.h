// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_PARTITIONALLOC_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_PARTITIONALLOC_SHIMS_H_

#include "components/gwp_asan/client/export.h"

namespace gwp_asan::internal::lud {

GWP_ASAN_EXPORT void InstallPartitionAllocHooks();

}  // namespace gwp_asan::internal::lud

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_PARTITIONALLOC_SHIMS_H_
