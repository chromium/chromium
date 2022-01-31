// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SWITCHES_H_
#define COMPONENTS_SYNC_SESSIONS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// Enables syncing Sessions data type in case when the window doesn't have open
// tabs anymore.
constexpr inline base::Feature kSyncConsiderEmptyWindowsSyncable{
    "SyncConsiderEmptyWindowsSyncable", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches

#endif  // COMPONENTS_SYNC_SESSIONS_SWITCHES_H_
