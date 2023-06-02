// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_MGLRU_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_MGLRU_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash {

// The MGLRU experimental feature.
BASE_DECLARE_FEATURE(kMGLRUEnable);

// The enable parameter used for MGLRU.
extern const base::FeatureParam<int> kMGLRUEnableValue;

// InitializeMGLRU will attempt to configure MGLRU with the experimental
// parameters for this user.
COMPONENT_EXPORT(ASH_MEMORY) void InitializeMGLRU();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_MGLRU_H_
