// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_PREF_NAMES_H_
#define COMPONENTS_METRICS_METRICS_PREF_NAMES_H_

namespace metrics {
namespace prefs {

// Alphabetical list of preference names specific to the metrics
// component. Document each in the .cc file.
extern const char kInstallDate[];
extern const char kMetricsClientID[];
extern const char kMetricsDefaultOptIn[];
extern const char kMetricsInitialLogs[];
extern const char kMetricsLowEntropySource[];
extern const char kMetricsOldLowEntropySource[];
extern const char kMetricsMachineId[];
extern const char kMetricsOngoingLogs[];
extern const char kMetricsResetIds[];

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
extern const char kStabilityBreakpadRegistrationFail[];
extern const char kStabilityBreakpadRegistrationSuccess[];
extern const char kStabilityBrowserLastLiveTimeStamp[];
extern const char kStabilityChildProcessCrashCount[];
extern const char kStabilityCrashCount[];
extern const char kStabilityCrashCountDueToGmsCoreUpdate[];
extern const char kStabilityCrashCountWithoutGmsCoreUpdateObsolete[];
extern const char kStabilityDebuggerNotPresent[];
extern const char kStabilityDebuggerPresent[];
extern const char kStabilityDeferredCount[];
extern const char kStabilityDiscardCount[];
extern const char kStabilityExecutionPhase[];
extern const char kStabilityExitedCleanly[];
extern const char kStabilityExtensionRendererCrashCount[];
extern const char kStabilityExtensionRendererFailedLaunchCount[];
extern const char kStabilityExtensionRendererLaunchCount[];
extern const char kStabilityGmsCoreVersion[];
extern const char kStabilityGpuCrashCount[];
extern const char kStabilityIncompleteSessionEndCount[];
extern const char kStabilityLaunchCount[];
extern const char kStabilityPageLoadCount[];
extern const char kStabilityRendererCrashCount[];
extern const char kStabilityRendererFailedLaunchCount[];
extern const char kStabilityRendererHangCount[];
extern const char kStabilityRendererLaunchCount[];
extern const char kStabilitySavedSystemProfile[];
extern const char kStabilitySavedSystemProfileHash[];
extern const char kStabilitySessionEndCompleted[];
extern const char kStabilityStatsBuildTime[];
extern const char kStabilityStatsVersion[];
extern const char kStabilitySystemCrashCount[];
extern const char kStabilityVersionMismatchCount[];

// Preferences for generating metrics at uninstall time.
extern const char kUninstallLaunchCount[];
extern const char kUninstallMetricsPageLoadCount[];
extern const char kUninstallMetricsUptimeSec[];

// For measuring data use for throttling UMA log uploads on cellular.
extern const char kUkmCellDataUse[];
extern const char kUmaCellDataUse[];
extern const char kUserCellDataUse[];

}  // namespace prefs
}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_PREF_NAMES_H_
