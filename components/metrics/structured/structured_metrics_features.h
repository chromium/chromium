// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace metrics::features {
// This is forward-declared since this file cannot have a direct dependency on
// //components/metrics to avoid circular dependencies. This feature is defined
// in //components/metrics/metrics_features.cc.
BASE_DECLARE_FEATURE(kStructuredMetrics);

}  // namespace metrics::features

namespace metrics::structured {

// Controls whether event sequence logging is enabled or not.
BASE_DECLARE_FEATURE(kEventSequenceLogging);

BASE_DECLARE_FEATURE(kBluetoothSessionizedMetrics);

// Controls whether fast pair logging is enabled or not.
BASE_DECLARE_FEATURE(kFastPairMetrics);

// Controls whether nearby share logging is enabled or not.
BASE_DECLARE_FEATURE(kNearbyShareMetrics);

// Controls whether the structured metrics service is recorder instead of the
// provider.
BASE_DECLARE_FEATURE(kEnabledStructuredMetricsService);

// Controls whether Phone Hub Structured Metrics is enabled or not.
BASE_DECLARE_FEATURE(kPhoneHubStructuredMetrics);

// Controls the minimum number of logs to be stored.
extern const base::FeatureParam<int> kMinLogQueueCount;

// Controls the minimum size of all logs that can be stored in bytes.
extern const base::FeatureParam<int> kMinLogQueueSizeBytes;

// Controls the maximum size of a single log in bytes.
extern const base::FeatureParam<int> kMaxLogSizeBytes;

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

// Returns the upload cadence in minutes for which events are uploaded to the
// UMA service to either be persisted as logs or uploaded.
int GetUploadCadenceMinutes();

// Returns the KiB proto limit per log. Events will not be added if the current
// log exceeds the proto limit and events will be dropped if exceeded.
int GetProtoKiBLimit();

// Returns the parameter used to control what projects are allowed to be
// recorded.
std::string GetDisabledProjects();

// Retrieves the Structured Metrics upload interval (defaults to 40 minutes).
int GetUploadInterval();

// Retrieves the collection interval for external metrics (defaults to 10
// minutes).
base::TimeDelta GetExternalMetricsCollectionInterval();

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_FEATURES_H_
