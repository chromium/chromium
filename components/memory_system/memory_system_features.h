// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_SYSTEM_MEMORY_SYSTEM_FEATURES_H_
#define COMPONENTS_MEMORY_SYSTEM_MEMORY_SYSTEM_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace memory_system::features {

BASE_DECLARE_FEATURE(kAllocationTraceRecorder);
BASE_DECLARE_FEATURE_PARAM(bool, kAllocationTraceRecorderForceAllProcesses);

}  // namespace memory_system::features

#endif  // COMPONENTS_MEMORY_SYSTEM_MEMORY_SYSTEM_FEATURES_H_
