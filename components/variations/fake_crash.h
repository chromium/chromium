// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_FAKE_CRASH_H_
#define COMPONENTS_VARIATIONS_FAKE_CRASH_H_

#include "base/component_export.h"

namespace variations {
// Schedules a crash dump to be uploaded (without crashing), if a
// VariationsFakeCrashAfterStartup is enabled. This enabled verification of
// safety measure for detecting crashes caused by features.
COMPONENT_EXPORT(VARIATIONS) void MaybeScheduleFakeCrash();
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_FAKE_CRASH_H_