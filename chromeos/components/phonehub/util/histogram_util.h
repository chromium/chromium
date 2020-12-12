// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_

namespace chromeos {
namespace phonehub {
namespace util {

// Enumeration of possible opt-in entry points for Phone Hub feature. Keep in
// sync with corresponding enum in tools/metrics/histograms/enums.xml. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class OptInEntryPoint {
  kSetupFlow = 0,
  kOnboardingFlow = 1,
  kSettings = 2,
  kMaxValue = kSettings,
};

// Logs a given opt-in |entry_point| for the PhoneHub feature.
void LogFeatureOptInEntryPoint(OptInEntryPoint entry_point);

}  // namespace util
}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_
