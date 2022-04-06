// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESS_CODE_CAST_COMMON_ACCESS_CODE_CAST_METRICS_H_
#define COMPONENTS_ACCESS_CODE_CAST_COMMON_ACCESS_CODE_CAST_METRICS_H_

// NOTE: Do not renumber enums as that would confuse interpretation of
// previously logged data. When making changes, also update the enum list
// in tools/metrics/histograms/enums.xml to keep it in sync.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccessCodeCastDialogOpenLocation {
  kBrowserCastMenu = 0,
  kSystemTrayCastFeaturePod = 1,
  kSystemTrayCastMenu = 2,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kSystemTrayCastMenu
};

class AccessCodeCastMetrics {
 public:
  AccessCodeCastMetrics();
  ~AccessCodeCastMetrics();

  // UMA histogram names.
  static const char kHistogramDialogOpenLocation[];

  // Records where the user clicked to open the AccessCodeCast dialog.
  static void RecordDialogOpenLocation(
      AccessCodeCastDialogOpenLocation location);
};

#endif  // COMPONENTS_ACCESS_CODE_CAST_COMMON_ACCESS_CODE_CAST_METRICS_H_
