// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_switches.h"

namespace metrics {
namespace switches {

// Enables the recording of metrics reports but disables reporting. In contrast
// to kForceEnableMetricsReporting, this executes all the code that a normal
// client would use for reporting, except the report is dropped rather than sent
// to the server. This is useful for finding issues in the metrics code during
// UI and performance tests.
const char kMetricsRecordingOnly[] = "metrics-recording-only";

// Override the standard time interval between each metrics report upload for
// UMA and UKM. It is useful to set to a short interval for debugging. Unit in
// seconds. (The default is 1800 seconds on desktop).
const char kMetricsUploadIntervalSec[] = "metrics-upload-interval";

// Forces a reset of the one-time-randomized FieldTrials on this client, also
// known as the Chrome Variations state.
const char kResetVariationState[] = "reset-variation-state";

// Forces metrics reporting to be enabled.
const char kForceEnableMetricsReporting[] = "force-enable-metrics-reporting";

}  // namespace switches
}  // namespace metrics
