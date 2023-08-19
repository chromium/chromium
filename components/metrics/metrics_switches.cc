// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_switches.h"

#include "base/check.h"
#include "base/command_line.h"

namespace metrics {
namespace switches {

// Enables the observing of all UMA logs created during the session and
// automatically exports them to the passed file path on shutdown (the file is
// created if it does not already exist). This also enables viewing all UMA logs
// in the chrome://metrics-internals debug page. The format of the exported file
// is outlined in MetricsServiceObserver::ExportLogsAsJson().
// Example usage: --export-uma-logs-to-file=/tmp/logs.json
const char kExportUmaLogsToFile[] = "export-uma-logs-to-file";

// Forces metrics reporting to be enabled. Should not be used for tests as it
// will send data to servers.
const char kForceEnableMetricsReporting[] = "force-enable-metrics-reporting";

// Forces MSBB setting to be on for UKM recording. Should only be used in
// automated testing browser sessions in which it is infeasible or impractical
// to toggle the setting manually.
const char kForceMsbbSettingOnForUkm[] = "force-msbb-setting-on-for-ukm";

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

// Overrides the URL of the server that UKM reports are uploaded to. This can
// only be used in debug builds.
const char kUkmServerUrl[] = "ukm-server-url";

// Overrides the URL of the server that UMA reports are uploaded to. This can
// only be used in debug builds.
const char kUmaServerUrl[] = "uma-server-url";

// Overrides the URL of the server that UMA reports are uploaded to when the
// connection to the default secure URL fails (see |kUmaServerUrl|). This can
// only be used in debug builds.
const char kUmaInsecureServerUrl[] = "uma-insecure-server-url";

}  // namespace switches

bool IsMetricsRecordingOnlyEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kMetricsRecordingOnly);
}

bool IsMetricsReportingForceEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceEnableMetricsReporting);
}

bool IsMsbbSettingForcedOnForUkm() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceMsbbSettingOnForUkm);
}

void EnableMetricsRecordingOnlyForTesting(base::CommandLine* command_line) {
  CHECK(command_line);
  if (!command_line->HasSwitch(switches::kMetricsRecordingOnly))
    command_line->AppendSwitch(switches::kMetricsRecordingOnly);
}

void ForceEnableMetricsReportingForTesting() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  CHECK(command_line);
  if (!command_line->HasSwitch(switches::kForceEnableMetricsReporting))
    command_line->AppendSwitch(switches::kForceEnableMetricsReporting);
}

}  // namespace metrics
