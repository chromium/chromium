// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_KSTALED_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_KSTALED_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash {

// The Kstaled experimental feature.
BASE_DECLARE_FEATURE(kKstaled);

// The ratio parameter used for kstaled.
extern const base::FeatureParam<int> kKstaledRatio;

// InitializeKstaled will attempt to configure kstaled with the experimental
// parameters for this user.
COMPONENT_EXPORT(ASH_MEMORY) void InitializeKstaled();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_KSTALED_H_
