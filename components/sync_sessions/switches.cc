// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/switches.h"

namespace switches {

// Enables syncing Sessions data type in case when the window doesn't have open
// tabs anymore.
const base::Feature kSyncConsiderEmptyWindowsSyncable{
    "SyncConsiderEmptyWindowsSyncable", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches
