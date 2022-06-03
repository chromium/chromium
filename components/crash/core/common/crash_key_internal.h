// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_INTERNAL_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_INTERNAL_H_

#include "components/crash/core/common/crash_export.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/breakpad/breakpad/src/common/simple_string_dictionary.h"

namespace crash_reporter {
namespace internal {

using TransitionalCrashKeyStorage =
    google_breakpad::NonAllocatingMap<kCrashKeyStorageKeySize,
                                      kCrashKeyStorageValueSize,
                                      kCrashKeyStorageNumEntries>;

// Accesses the underlying storage for crash keys for non-Crashpad clients.
CRASH_KEY_EXPORT TransitionalCrashKeyStorage* GetCrashKeyStorage();

CRASH_KEY_EXPORT void ResetCrashKeyStorageForTesting();

}  // namespace internal
}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_INTERNAL_H_
