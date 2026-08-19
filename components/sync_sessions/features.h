// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_FEATURES_H_
#define COMPONENTS_SYNC_SESSIONS_FEATURES_H_

#include "base/feature_list.h"

namespace sync_sessions {

BASE_DECLARE_FEATURE(kOptimizeAssociateWindowsAndroid);
BASE_DECLARE_FEATURE(kFilterNavigationsBySyncSessionsClient);
// When enabled, the session name (display name for the synced device) is
// enriched using preferred names from DEVICE_INFO instead of falling back to
// the raw hardware model.
BASE_DECLARE_FEATURE(kSyncSessionsUsePreferredDisplayName);

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_FEATURES_H_
