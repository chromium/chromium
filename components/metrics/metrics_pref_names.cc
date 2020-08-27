// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_pref_names.h"

namespace metrics {
namespace prefs {

// Set once, to the current epoch time, on the first run of chrome on this
// machine. Attached to metrics reports forever thereafter.
const char kInstallDate[] = "uninstall_metrics.installation_date2";

// The metrics client GUID.
// Note: The name client_id2 is a result of creating
// new prefs to do a one-time reset of the previous values.
const char kMetricsClientID[] = "user_experience_metrics.client_id2";

// An enum value indicating the default value of the enable metrics reporting
// checkbox shown during first-run. If it's opt-in, then the checkbox defaulted
// to unchecked, if it's opt-out, then it defaulted to checked. This value is
// only recorded during first-run, so older clients will not set it. The enum
// used for the value is metrics::MetricsServiceClient::EnableMetricsDefault.
const char kMetricsDefaultOptIn[] = "user_experience_metrics.default_opt_in";

// Array of dictionaries that are each UMA logs that were supposed to be sent in
// the first minute of a browser session. These logs include things like crash
// count info, etc.
const char kMetricsInitialLogs[] = "user_experience_metrics.initial_logs2";

// An dictionary of information about the unsent initial logs, it was
// recorded when the unsent log is persisted and will be written into the
// metrics at the next browser starts up.
const char kMetricsInitialLogsMetadata[] =
    "user_experience_metrics.unsent_log_metadata.initial_logs";

// Low entropy source values. The new source (with suffix "3") was created
// because the old source (with suffix "2") is biased in the wild. Clients which
// have an old source still incorporate it into the high entropy source, to
// avoid reshuffling experiments using high entropy, but use the new source for
// experiments requiring low entropy. Newer clients only have the new source,
// and use it both for low entropy experiments to to incorporate into the high
// entropy source for high entropy experiments.
const char kMetricsLowEntropySource[] =
    "user_experience_metrics.low_entropy_source3";
const char kMetricsOldLowEntropySource[] =
    "user_experience_metrics.low_entropy_source2";

// A machine ID used to detect when underlying hardware changes. It is only
// stored locally and never transmitted in metrics reports.
const char kMetricsMachineId[] = "user_experience_metrics.machine_id";

// Array of dictionaries that are each UMA logs that were not sent because the
// browser terminated before these accumulated metrics could be sent. These
// logs typically include histograms and memory reports, as well as ongoing
// user activities.
const char kMetricsOngoingLogs[] = "user_experience_metrics.ongoing_logs2";

// An dictionary that is same as kUnsentLogMetkMetricsInitialLogsMetadata,
// but for the ongoing logs.
const char kMetricsOngoingLogsMetadata[] =
    "user_experience_metrics.unsent_log_metadata.ongoing_logs";

// Boolean that indicates a cloned install has been detected and the metrics
// client id and low entropy source should be reset.
const char kMetricsResetIds[] = "user_experience_metrics.reset_metrics_ids";

// Boolean that specifies whether or not crash reporting and metrics reporting
// are sent over the network for analysis.
const char kMetricsReportingEnabled[] =
    "user_experience_metrics.reporting_enabled";

// Date/time when the user opted in to UMA and generated the client id most
// recently (local machine time, stored as a 64-bit time_t value).
const char kMetricsReportingEnabledTimestamp[] =
    "user_experience_metrics.client_id_timestamp";

// The metrics client session ID.
const char kMetricsSessionID[] = "user_experience_metrics.session_id";

// The prefix of the last-seen timestamp for persistent histogram files.
// Values are named for the files themselves.
const char kMetricsLastSeenPrefix[] =
    "user_experience_metrics.last_seen.";

// Array of the number of samples in the memory mapped file.
const char kMetricsFileMetricsMetadata[] =
    "user_experience_metrics.file_metrics_metadata";

// Number of times the browser has been able to register crash reporting.
const char kStabilityBreakpadRegistrationSuccess[] =
    "user_experience_metrics.stability.breakpad_registration_ok";

// Number of times the browser has failed to register crash reporting.
const char kStabilityBreakpadRegistrationFail[] =
    "user_experience_metrics.stability.breakpad_registration_fail";

// A time stamp at which time the browser was known to be alive. Used to
// evaluate whether the browser crash was due to a whole system crash.
// At minimum this is updated each time the "exited_cleanly" preference is
// modified, but can also be optionally updated on a slow schedule.
const char kStabilityBrowserLastLiveTimeStamp[] =
    "user_experience_metrics.stability.browser_last_live_timestamp";

// Total number of child process crashes (other than renderer / extension
// renderer ones, and plugin children, which are counted separately) since the
// last report.
const char kStabilityChildProcessCrashCount[] =
    "user_experience_metrics.stability.child_process_crash_count";

// Number of times the application exited uncleanly since the last report.
// On Android this does not count the ones due to Gms Core updates (below).
const char kStabilityCrashCount[] =
    "user_experience_metrics.stability.crash_count";

// Number of times the application exited uncleanly since the last report
// due to a gms core update.
const char kStabilityCrashCountDueToGmsCoreUpdate[] =
    "user_experience_metrics.stability.crash_count_due_to_gms_core_update";

// Number of times the initial stability log upload was deferred to the next
// startup.
const char kStabilityDeferredCount[] =
    "user_experience_metrics.stability.deferred_count";

// Number of times stability data was discarded. This is accumulated since the
// last report, even across versions.
const char kStabilityDiscardCount[] =
    "user_experience_metrics.stability.discard_count";

// Number of times the browser has been run under a debugger.
const char kStabilityDebuggerPresent[] =
    "user_experience_metrics.stability.debugger_present";

// Number of times the browser has not been run under a debugger.
const char kStabilityDebuggerNotPresent[] =
    "user_experience_metrics.stability.debugger_not_present";

// True if the previous run of the program exited cleanly.
const char kStabilityExitedCleanly[] =
    "user_experience_metrics.stability.exited_cleanly";

// Number of times an extension renderer process crashed since the last report.
const char kStabilityExtensionRendererCrashCount[] =
    "user_experience_metrics.stability.extension_renderer_crash_count";

// Number of times an extension renderer process failed to launch since the last
// report.
const char kStabilityExtensionRendererFailedLaunchCount[] =
    "user_experience_metrics.stability.extension_renderer_failed_launch_count";

// Number of times an extension renderer process successfully launched since the
// last report.
const char kStabilityExtensionRendererLaunchCount[] =
    "user_experience_metrics.stability.extension_renderer_launch_count";

// The total number of samples that will be lost if ASSOCIATE_INTERNAL_PROFILE
// isn't enabled since the previous stability recorded, this is different than
// the previous browser run, because one file was just uploaded before the
// stability is recorded.
const char kStabilityFileMetricsUnsentSamplesCount[] =
    "user_experience_metrics.stability.file_metrics_unsent_samples_count";

// The number of the unsent files at the time the stability recorded.
const char kStabilityFileMetricsUnsentFilesCount[] =
    "user_experience_metrics.stability.file_metrics_unsent_files_count";

// The GMS core version used in Chrome.
const char kStabilityGmsCoreVersion[] =
    "user_experience_metrics.stability.gms_core_version";

// Number of times a gpu process crashed since the last report. Currently only
// recorded on Android.
const char kStabilityGpuCrashCount[] =
    "user_experience_metrics.stability.gpu_crash_count";

// Number of times the session end did not complete.
const char kStabilityIncompleteSessionEndCount[] =
    "user_experience_metrics.stability.incomplete_session_end_count";

// Number of times the application was launched since last report.
const char kStabilityLaunchCount[] =
    "user_experience_metrics.stability.launch_count";

// Number of times a page load event occurred since the last report.
const char kStabilityPageLoadCount[] =
    "user_experience_metrics.stability.page_load_count";

// Number of times a renderer process crashed since the last report.
const char kStabilityRendererCrashCount[] =
    "user_experience_metrics.stability.renderer_crash_count";

// Number of times a renderer process failed to launch since the last report.
const char kStabilityRendererFailedLaunchCount[] =
    "user_experience_metrics.stability.renderer_failed_launch_count";

// Number of times the renderer has become non-responsive since the last
// report.
const char kStabilityRendererHangCount[] =
    "user_experience_metrics.stability.renderer_hang_count";

// Number of times a renderer process successfully launched since the last
// report.
const char kStabilityRendererLaunchCount[] =
    "user_experience_metrics.stability.renderer_launch_count";

// Base64 encoded serialized UMA system profile proto from the previous session.
const char kStabilitySavedSystemProfile[] =
    "user_experience_metrics.stability.saved_system_profile";

// SHA-1 hash of the serialized UMA system profile proto (hex encoded).
const char kStabilitySavedSystemProfileHash[] =
    "user_experience_metrics.stability.saved_system_profile_hash";

// False if we received a session end and either we crashed during processing
// the session end or ran out of time and windows terminated us.
const char kStabilitySessionEndCompleted[] =
    "user_experience_metrics.stability.session_end_completed";

// Build time, in seconds since an epoch, which is used to assure that stability
// metrics reported reflect stability of the same build.
const char kStabilityStatsBuildTime[] =
    "user_experience_metrics.stability.stats_buildtime";

// Version string of previous run, which is used to assure that stability
// metrics reported under current version reflect stability of the same version.
const char kStabilityStatsVersion[] =
    "user_experience_metrics.stability.stats_version";

// Number of times the application exited uncleanly and the system session
// embedding the browser session ended abnormally since the last report.
// Windows only.
const char kStabilitySystemCrashCount[] =
    "user_experience_metrics.stability.system_crash_count";

// Number of times the version number stored in prefs did not match the
// serialized system profile version number.
const char kStabilityVersionMismatchCount[] =
    "user_experience_metrics.stability.version_mismatch_count";

// The keys below are strictly increasing counters over the lifetime of
// a chrome installation. They are (optionally) sent up to the uninstall
// survey in the event of uninstallation.
const char kUninstallLaunchCount[] = "uninstall_metrics.launch_count";
const char kUninstallMetricsPageLoadCount[] =
    "uninstall_metrics.page_load_count";
const char kUninstallMetricsUptimeSec[] = "uninstall_metrics.uptime_sec";

// Dictionary for measuring cellular data used by UKM service during last 7
// days.
const char kUkmCellDataUse[] = "user_experience_metrics.ukm_cell_datause";

// Dictionary for measuring cellular data used by UMA service during last 7
// days.
const char kUmaCellDataUse[] = "user_experience_metrics.uma_cell_datause";

// Dictionary for measuring cellular data used by user including chrome services
// per day.
const char kUserCellDataUse[] = "user_experience_metrics.user_call_datause";

}  // namespace prefs
}  // namespace metrics
