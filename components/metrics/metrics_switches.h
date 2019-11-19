// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SWITCHES_H_
#define COMPONENTS_METRICS_METRICS_SWITCHES_H_

namespace metrics {
namespace switches {

// Alphabetical list of switches specific to the metrics component. Document
// each in the .cc file.

extern const char kMetricsRecordingOnly[];
extern const char kMetricsUploadIntervalSec[];
extern const char kResetVariationState[];
extern const char kForceEnableMetricsReporting[];

}  // namespace switches
}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SWITCHES_H_
