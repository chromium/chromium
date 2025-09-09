// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_TIMESTAMPED_PREF_VALUE_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_TIMESTAMPED_PREF_VALUE_H_

#include "base/time/time.h"
#include "base/values.h"

namespace sync_preferences {

// Holds a Pref's `value` and the time of its last observed change.
//
// The timestamp is synced across devices. For a Pref that was not previously
// synced, the timestamp is taken from the first recorded change after the user
// started syncing Prefs. If no change has been recorded, the timestamp will be
// unset.
struct TimestampedPrefValue {
  base::Value value;
  base::Time last_observed_change_time;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_TIMESTAMPED_PREF_VALUE_H_
