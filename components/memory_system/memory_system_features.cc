// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/memory_system_features.h"

namespace memory_system::features {

BASE_FEATURE(kAllocationTraceRecorder,
             "AllocationTraceRecorder",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace memory_system::features
