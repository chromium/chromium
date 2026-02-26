// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/features.h"

namespace sync_preferences::features {

BASE_FEATURE(kEnableCrossDevicePrefTracker,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kCrossDevicePrefTrackerExtraLogs,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace sync_preferences::features
