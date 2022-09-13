// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_

#include "base/feature_list.h"

namespace metrics {
namespace structured {

// This can be used to disable structured metrics as a whole.
extern const base::Feature kStructuredMetrics;

// Controls whether CrOSEvents logging is enabled or not.
extern const base::Feature kCrOSEvents;

extern const base::Feature kBluetoothSessionizedMetrics;

// Delays appending structured metrics events until HWID has been loaded.
extern const base::Feature kDelayUploadUntilHwid;

// TODO(crbug.com/1148168): This is a temporary switch to revert structured
// metrics upload to its old behaviour. Old behaviour:
// - all metrics are uploaded in the main UMA upload
//
// New behaviour:
// - Projects with id type 'uma' are uploaded in the main UMA uploaded
// - Projects with id type 'project-id' or 'none' are uploaded independently.
//
// Once we are comfortable with this change, this parameter can be removed.
bool IsIndependentMetricsUploadEnabled();

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_
