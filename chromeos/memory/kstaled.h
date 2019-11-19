// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_KSTALED_H_
#define CHROMEOS_MEMORY_KSTALED_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// The Kstaled experimental feature.
extern const base::Feature kKstaled;

// The ratio parameter used for kstaled.
extern const base::FeatureParam<int> kKstaledRatio;

// InitializeKstaled will attempt to configure kstaled with the experimental
// parameters for this user.
CHROMEOS_EXPORT void InitializeKstaled();

}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_KSTALED_H_
