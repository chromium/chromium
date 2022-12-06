// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_

#include "base/feature_list.h"

namespace metrics {
namespace structured {

// This can be used to disable structured metrics as a whole.
BASE_DECLARE_FEATURE(kStructuredMetrics);

// Controls whether event sequence logging is enabled or not.
BASE_DECLARE_FEATURE(kEventSequenceLogging);

BASE_DECLARE_FEATURE(kBluetoothSessionizedMetrics);

// Delays appending structured metrics events until HWID has been loaded.
BASE_DECLARE_FEATURE(kDelayUploadUntilHwid);

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

// Returns the parameter used to control how many files will be read into memory
// before events start being discarded.
//
// This is to prevent too many files to be read into memory, causing Chrome to
// OOM.
int GetFileLimitPerScan();

// Returns the parameter used to control the max size of an event. Any event
// exceeding this memory limit will be discarded. Defaults to 50KB.
int GetFileSizeByteLimit();

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_
