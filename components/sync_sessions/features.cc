// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/features.h"

namespace sync_sessions {

BASE_FEATURE(kOptimizeAssociateWindowsAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFilterNavigationsBySyncSessionsClient,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncTabScreenshots, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace sync_sessions
