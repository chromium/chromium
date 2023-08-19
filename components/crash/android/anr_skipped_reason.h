// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_ANDROID_ANR_SKIPPED_REASON_H_
#define COMPONENTS_CRASH_ANDROID_ANR_SKIPPED_REASON_H_

// Enum to record how many and why ANRs were not uploaded to UMA.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.crash.anr
enum class AnrSkippedReason {
  kFilesystemReadFailure = 0,
  kFilesystemWriteFailure = 1,
  kMissingVersion = 2,
  kOnlyMissingNative = 3,
  kNotSkipped = 4,
  kMaxValue = kNotSkipped,
};

#endif  // COMPONENTS_CRASH_ANDROID_ANR_SKIPPED_REASON_H_
