// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_MEMORY_H_
#define CHROMEOS_MEMORY_MEMORY_H_

#include <cstddef>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// MlockMaping will attempt to mlock a mapping using the newer mlock2 syscall
// if available using the MLOCK_ONFAULT option. This will allow pages to be
// locked as they are faulted in. If the running kernel does not support
// mlock2, it was added in kernel 4.4, it will fall back to mlock where it
// will lock all pages immediately by faulting them in.
CHROMEOS_EXPORT bool MlockMapping(void* addr, size_t length);

// A feature which controls the locking the main program text.
extern const base::Feature kCrOSLockMainProgramText;

// The maximum number of bytes that the browser will attempt to lock, -1 will
// disable the max size and is the default option.
extern const base::FeatureParam<int> kCrOSLockMainProgramTextMaxSize;

// Lock main program text segments fully.
CHROMEOS_EXPORT void LockMainProgramText();

// It should be called when some memory configuration is changed.
CHROMEOS_EXPORT void UpdateMemoryParameters();

}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_MEMORY_H_
