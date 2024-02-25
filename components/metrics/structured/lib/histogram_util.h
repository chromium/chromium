// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_HISTOGRAM_UTIL_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_HISTOGRAM_UTIL_H_

namespace metrics::structured {

// Describes the action taken by KeyData::ValidateAndGetKey on a particular user
// event key. A key can either be valid with no action taken, missing and so
// created, or out of its rotation period and so re-created. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class KeyValidationState {
  kValid = 0,
  kCreated = 1,
  kRotated = 2,
  kMaxValue = kRotated,
};

// Possible internal errors of the structured metrics system. These are events
// we expect to never see, so only the absolute counts should be looked at, the
// bucket proportion doesn't make sense. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class StructuredMetricsError {
  kMissingKey = 0,
  kWrongKeyLength = 1,
  kMissingLastRotation = 2,
  kMissingRotationPeriod = 3,
  kFailedUintConversion = 4,
  kKeyReadError = 5,
  kKeyParseError = 6,
  kKeyWriteError = 7,
  kKeySerializationError = 8,
  kEventReadError = 9,
  kEventParseError = 10,
  kEventWriteError = 11,
  kEventSerializationError = 12,
  kUninitializedClient = 13,
  kInvalidEventParsed = 14,
  kMaxValue = kInvalidEventParsed,
};

// Logs an error state in Structured metrics.
void LogInternalError(StructuredMetricsError error);

// Logs the key validation state. This captures the key state when keys are
// requested to be validated.
void LogKeyValidation(KeyValidationState state);

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_HISTOGRAM_UTIL_H_
