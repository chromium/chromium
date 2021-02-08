// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_HISTOGRAM_UTIL_H_
#define COMPONENTS_METRICS_STRUCTURED_HISTOGRAM_UTIL_H_

#include <string>
#include <vector>

#include "components/prefs/persistent_pref_store.h"

namespace metrics {
namespace structured {

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
  kMaxValue = kFailedUintConversion,
};

// Whether a single event was recorded correctly, or otherwise what error state
// occurred. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class EventRecordingState {
  kRecorded = 0,
  kProviderUninitialized = 1,
  kRecordingDisabled = 2,
  kProviderMissing = 3,
  kMaxValue = kProviderMissing,
};

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

void LogInternalError(StructuredMetricsError error);

// Log an error on reading the JSONPrefStore from disk. A
// PREF_READ_ERROR_NO_FILE is expected when a profile first logs in on a
// particular device.
void LogPrefReadError(PersistentPrefStore::PrefReadError error);

void LogEventRecordingState(EventRecordingState state);

void LogKeyValidation(KeyValidationState state);

// Log how many structured metrics events were contained in a call to
// ProvideCurrentSessionData.
void LogNumEventsInUpload(int num_events);

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_HISTOGRAM_UTIL_H_
