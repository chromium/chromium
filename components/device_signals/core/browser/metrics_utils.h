// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_METRICS_UTILS_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_METRICS_UTILS_H_

#include <optional>

#include "base/time/time.h"

namespace device_signals {

enum class SignalCollectionError;
enum class SignalName;
enum class UserPermission;

// Set of possible errors encountered when parsing some signal values from a
// file or other data source. Do not reorder the values. Also change
// DeviceSignalsParsingError in enums.xml if adding new values here.
enum class SignalsParsingError {
  kHitMaxDataSize = 0,
  kDataMalformed = 1,
  kBase64DecodingFailed = 2,
  kJsonParsingFailed = 3,
  kMissingRequiredProperty = 4,
  kMaxValue = kMissingRequiredProperty
};

// Records that `permission` was the outcome of a permission check.
void LogUserPermissionChecked(UserPermission permission);

// Records that a request to collect `signal_name` was received.
void LogSignalCollectionRequested(SignalName signal_name);

// Records that a request to collect the parameterized signal named
// `signal_name` was received with `number_of_items` parameters.
void LogSignalCollectionRequestedWithItems(SignalName signal_name,
                                           size_t number_of_items);

// Records that the collection of the signal `signal_name` failed with `error`,
// which, based on `is_top_level_error`, is either a top-level aggregation
// error, or a collection-level error. `start_time` is the time at which the
// signal collection request was received.
void LogSignalCollectionFailed(SignalName signal_name,
                               base::TimeTicks start_time,
                               SignalCollectionError error,
                               bool is_top_level_error);

// Records that the collection of the signal `signal_name` was successful.
// If applicable, will also record the number of items collected as given by
// `signal_collection_size`. `start_time` is the time at which the
// signal collection request was received. `signal_request_size` is the number
// of items that were part of the signal collection request.
void LogSignalCollectionSucceeded(
    SignalName signal_name,
    base::TimeTicks start_time,
    std::optional<size_t> signal_collection_size,
    std::optional<size_t> signal_request_size = std::nullopt);

// Records that an error occurred when trying to parse signals from the
// CrowdStrike data.zta file.
void LogCrowdStrikeParsingError(SignalsParsingError error);

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_METRICS_UTILS_H_
