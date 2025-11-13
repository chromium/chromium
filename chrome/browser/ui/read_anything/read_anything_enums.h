// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. ReadAnythingOpenTrigger in
// tools/metrics/histograms/enums.xml should also be updated when changed
// here.
enum class ReadAnythingOpenTrigger {
  kAppMenu = 0,
  kMinValue = kAppMenu,
  kReadAnythingContextMenu = 1,
  kReadAnythingNavigationThrottle = 2,
  kMaxValue = kReadAnythingNavigationThrottle,
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_
