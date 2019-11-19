// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_GPU_VSYNC_CALLBACK_H_
#define COMPONENTS_VIZ_COMMON_GPU_GPU_VSYNC_CALLBACK_H_

#include "base/callback.h"
#include "base/time/time.h"

namespace viz {

using GpuVSyncCallback =
    base::RepeatingCallback<void(base::TimeTicks vsync_time,
                                 base::TimeDelta vsync_interval)>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_GPU_VSYNC_CALLBACK_H_
