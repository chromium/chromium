// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SWITCHES_H_
#define COMPONENTS_METRICS_METRICS_SWITCHES_H_

#include "base/command_line.h"

namespace metrics {
namespace switches {

// Alphabetical list of switches specific to the metrics component. Document
// each in the .cc file.

extern const char kExportUmaLogsToFile[];
extern const char kForceEnableMetricsReporting[];
extern const char kMetricsRecordingOnly[];
extern const char kMetricsUploadIntervalSec[];
extern const char kResetVariationState[];
extern const char kUkmServerUrl[];
extern const char kUmaServerUrl[];
extern const char kUmaInsecureServerUrl[];

}  // namespace switches

// Returns true if kMetricsRecordingOnly is on the command line for the current
// process.
bool IsMetricsRecordingOnlyEnabled();

// Returns true if kForceEnableMetricsReporting is on the command line for the
// current process.
bool IsMetricsReportingForceEnabled();

// Returns true if kForceMsbbSettingOnForUkm is on the command line for the
// current process.
bool IsMsbbSettingForcedOnForUkm();

// Adds kMetricsRecordingOnly to |command_line| if not already present.
void EnableMetricsRecordingOnlyForTesting(
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess());

// Adds kForceEnableMetricsReporting to |command_line| if not already present.
void ForceEnableMetricsReportingForTesting(
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess());

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SWITCHES_H_
