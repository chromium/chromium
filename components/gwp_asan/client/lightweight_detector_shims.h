// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHIMS_H_

#include <stddef.h>  // for size_t

#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/common/lightweight_detector_state.h"

namespace gwp_asan::internal {

GWP_ASAN_EXPORT void InstallLightweightDetectorHooks(
    LightweightDetectorMode lightweight_detector_mode,
    size_t num_lightweight_detector_metadata);

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHIMS_H_
