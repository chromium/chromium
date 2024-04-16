// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_PREF_NAMES_H_
#define COMPONENTS_METRICS_METRICS_PREF_NAMES_H_

#include "build/build_config.h"

namespace metrics {
namespace prefs {

// Alphabetical list of preference names specific to the metrics
// component. Document each in the .cc file.
extern const char kInstallDate[];
extern const char kMetricsClientID[];
extern const char kMetricsFileMetricsMetadata[];
extern const char kMetricsDefaultOptIn[];
extern const char kMetricsInitialLogs[];
extern const char kMetricsInitialLogsMetadata[];
extern const char kMetricsLimitedEntropyRandomizationSource[];
extern const char kMetricsLogFinalizedRecordId[];
extern const char kMetricsLogRecordId[];
extern const char kMetricsLowEntropySource[];
extern const char kMetricsOldLowEntropySource[];
extern const char kMetricsProvisionalClientID[];
extern const char kMetricsPseudoLowEntropySource[];
extern const char kMetricsMachineId[];
extern const char kMetricsOngoingLogs[];
extern const char kMetricsOngoingLogsMetadata[];
extern const char kMetricsResetIds[];
#if BUILDFLAG(IS_ANDROID)
extern const char kUsePostFREFixSamplingTrial[];
#endif  // BUILDFLAG(IS_ANDROID)

// Preferences for cloned installs.
extern const char kClonedResetCount[];
extern const char kFirstClonedResetTimestamp[];
extern const char kLastClonedResetTimestamp[];

// For finding out whether metrics and crash reporting is enabled use the
// relevant embedder-specific subclass of MetricsServiceAccessor instead of
// reading this pref directly; see the comments on metrics_service_accessor.h.
// (NOTE: If within //chrome, use
// ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()).
extern const char kMetricsReportingEnabled[];
extern const char kMetricsReportingEnabledTimestamp[];
extern const char kMetricsSessionID[];
extern const char kMetricsLastSeenPrefix[];

// Preferences for recording stability logs.
extern const char kStabilityBrowserLastLiveTimeStamp[];
extern const char kStabilityCrashCountDueToGmsCoreUpdate[];
// TODO(crbug.com/40194783): Remove this Local State pref once the new behavior
// launches on Android Chrome.
extern const char kStabilityExitedCleanly[];
extern const char kStabilityFileMetricsUnsentSamplesCount[];
extern const char kStabilityFileMetricsUnsentFilesCount[];
extern const char kStabilityGmsCoreVersion[];
#if BUILDFLAG(IS_ANDROID)
extern const char kStabilityLaunchCount[];
extern const char kStabilityPageLoadCount[];
extern const char kStabilityRendererLaunchCount[];
#endif
extern const char kStabilitySavedSystemProfile[];
extern const char kStabilitySavedSystemProfileHash[];
extern const char kStabilityStatsBuildTime[];
extern const char kStabilityStatsVersion[];
extern const char kStabilitySystemCrashCount[];

// For measuring data use for throttling UMA log uploads on cellular.
extern const char kUkmCellDataUse[];
extern const char kUmaCellDataUse[];
extern const char kUserCellDataUse[];

// For supporting per-user collection on Chrome OS.
extern const char kMetricsCurrentUserId[];

}  // namespace prefs
}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_PREF_NAMES_H_
