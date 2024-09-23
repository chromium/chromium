// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_STOP_SOURCE_H_
#define COMPONENTS_SYNC_BASE_STOP_SOURCE_H_

namespace syncer {

// Enumerate the main sources that can turn off sync. This enum is used to
// back a UMA histogram. These values are persisted to logs. Entries should not
// be renumbered and numeric values should never be reused. Keep in sync with
// SyncStopSource in tools/metrics/histograms/metadata/sync/enums.xml.
// LINT.IfChange(SyncStopSource)
enum StopSource {
  // Deprecated: PROFILE_DESTRUCTION = 0,
  SIGN_OUT = 1,              // The user signed out of Chrome.
  BIRTHDAY_ERROR = 2,        // A dashboard stop-and-clear on the server.
  CHROME_SYNC_SETTINGS = 3,  // The on/off switch in settings for mobile Chrome.
  // Deprecated: ANDROID_CHROME_SYNC = 4,
  // Deprecated: ANDROID_MASTER_SYNC = 5,
  STOP_SOURCE_LIMIT = 6,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncStopSource)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_STOP_SOURCE_H_
