// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_

#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

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

// Enumeration of results of a tethering connection attempt.
enum class TetherConnectionResult {
  kAttemptConnection = 0,
  kSuccess = 1,
  kMaxValue = kSuccess,
};

// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PhoneHubMessageResult {
  kRequestAttempted = 0,
  kResponseReceived = 1,
  kMaxValue = kResponseReceived,
};

// Logs a given opt-in |entry_point| for the PhoneHub feature.
void LogFeatureOptInEntryPoint(OptInEntryPoint entry_point);

// Logs a given |result| of a tethering connection attempt.
void LogTetherConnectionResult(TetherConnectionResult result);

// Logs a given |result| for a request message.
void LogMessageResult(proto::MessageType message, PhoneHubMessageResult result);

}  // namespace util
}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_
