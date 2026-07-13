// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STARTUP_VISIBILITY_H_
#define COMPONENTS_METRICS_STARTUP_VISIBILITY_H_

namespace metrics {

// Denotes whether this session is a background or foreground session at
// startup. May be unknown. A background session refers to the situation in
// which the browser process starts; does some work, e.g. servicing a sync; and
// ends without ever becoming visible. Note that the point in startup at which
// this value is determined is likely before the UI is visible.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class StartupVisibility {
  kUnknown = 0,
  kBackground = 1,
  kForeground = 2,
  kMaxValue = kForeground,
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_STARTUP_VISIBILITY_H_
