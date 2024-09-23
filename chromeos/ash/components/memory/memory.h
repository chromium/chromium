// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_MEMORY_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_MEMORY_H_

#include <cstddef>

#include "base/component_export.h"
#include "base/metrics/field_trial_params.h"

namespace ash {
// A feature which controls the locking the main program text.
BASE_DECLARE_FEATURE(kCrOSLockMainProgramText);

// The maximum number of bytes that the browser will attempt to lock, -1 will
// disable the max size and try to lock whole text.
extern const base::FeatureParam<int> kCrOSLockMainProgramTextMaxSize;

// Lock main program text segments fully.
COMPONENT_EXPORT(ASH_MEMORY) void LockMainProgramText();
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_MEMORY_H_
