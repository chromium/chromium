// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/features.h"

namespace browser_watcher {

BASE_FEATURE(kExtendedCrashReportingFeature,
             "ExtendedCrashReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kInMemoryOnlyParam[] = "in_memory_only";

}  // namespace browser_watcher
