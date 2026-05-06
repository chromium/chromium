// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_switches.h"

#include "base/check.h"
#include "base/command_line.h"

namespace metrics {

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
  if (!command_line->HasSwitch(switches::kMetricsRecordingOnly)) {
    command_line->AppendSwitch(switches::kMetricsRecordingOnly);
  }
}

void ForceEnableMetricsReportingForTesting() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  CHECK(command_line);
  if (!command_line->HasSwitch(switches::kForceEnableMetricsReporting)) {
    command_line->AppendSwitch(switches::kForceEnableMetricsReporting);
  }
}

}  // namespace metrics
