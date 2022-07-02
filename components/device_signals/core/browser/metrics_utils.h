// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_METRICS_UTILS_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_METRICS_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device_signals {

enum class SignalCollectionError;
enum class SignalName;
enum class UserPermission;

// Records that `permission` was the outcome of a permission check.
void LogUserPermissionChecked(UserPermission permission);

// Records that a request to collect `signal_name` was received.
void LogSignalCollectionRequested(SignalName signal_name);

// Records that the collection of the signal `signal_name` failed with `error`,
// which, based on `is_top_level_error`, is either a top-level aggregation
// error, or a collection-level error.
void LogSignalCollectionFailed(SignalName signal_name,
                               SignalCollectionError error,
                               bool is_top_level_error);

// Records that the collection of the signal `signal_name` was successful.
// If applicable, will also record the number of items collected as given by
// `signal_collection_size`.
void LogSignalCollectionSucceeded(
    SignalName signal_name,
    absl::optional<size_t> signal_collection_size);

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_METRICS_UTILS_H_
