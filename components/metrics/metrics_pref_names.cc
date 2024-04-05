// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_pref_names.h"

namespace metrics {
namespace prefs {

// Set once, to the current epoch time, on the first run of chrome on this
// machine. Attached to metrics reports forever thereafter.
// Note: the 'uninstall_metrics' name is a legacy name and doesn't mean much.
const char kInstallDate[] = "uninstall_metrics.installation_date2";

// A provisional metrics client GUID used for field trial group assignments
// before metrics reporting consent is known (i.e., during first run). This GUID
// is never reported directly. However, if the user enables UMA, this
// provisional client GUID becomes the metrics client GUID (see
// |kMetricsClientID|), and this pref is cleared. In that case, the GUID may
// be reported.
// Note: This GUID is stored in prefs because it is possible that the user
// closes Chrome during the FRE. We re-use this GUID in subsequent FRE runs
// until metrics reporting consent is truly known.
const char kMetricsProvisionalClientID[] =
    "user_experience_metrics.provisional_client_id";

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

// A serialized representation of a base::UnguessableToken, used for randomizing
// limited entropy field trials.
const char kMetricsLimitedEntropyRandomizationSource[] =
    "user_experience_metrics.limited_entropy_randomization_source";

// A counter tracking the most recently used finalized log record id. Increment
// this value by one (1) for each finalized log.
const char kMetricsLogFinalizedRecordId[] =
    "user_experience_metrics.log_finalized_record_id";

// A counter tracking the most recently used log record id. Increment this value
// by one (1) for each newly created log.
const char kMetricsLogRecordId[] = "user_experience_metrics.log_record_id";

// Low entropy source values. The new source (with suffix "3") was created
// because the old source (with suffix "2") is biased in the wild. Clients which
// have an old source still incorporate it into the high entropy source, to
// avoid reshuffling experiments using high entropy, but use the new source for
// experiments requiring low entropy. Newer clients only have the new source,
// and use it both for low entropy experiments to to incorporate into the high
// entropy source for high entropy experiments. The pseudo low entropy source
// is not used for trial assignment, but only for statistical validation. It
// should be assigned in the same way as the new source (with suffix "3").
const char kMetricsLowEntropySource[] =
    "user_experience_metrics.low_entropy_source3";
const char kMetricsOldLowEntropySource[] =
    "user_experience_metrics.low_entropy_source2";
const char kMetricsPseudoLowEntropySource[] =
    "user_experience_metrics.pseudo_low_entropy_source";

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

#if BUILDFLAG(IS_ANDROID)
// Boolean that determines whether to use the new sampling trial
// "PostFREFixMetricsAndCrashSampling" and feature "PostFREFixMetricsReporting"
// to control sampling on Android Chrome. This is set to true when disabling
// metrics reporting, or on start up if metrics reporting is not consented to
// (including new users going through their first run). As a result, all new UMA
// users should have this pref set to true.
// Note: This exists due to a bug in which the old sampling rate was not being
// applied correctly. In order for the fix to not affect the overall sampling
// rate, this pref controls what trial/feature to use to determine whether the
// client is sampled. See crbug/1306481.
const char kUsePostFREFixSamplingTrial[] =
    "user_experience_metrics.use_post_fre_fix_sampling_trial";
#endif  // BUILDFLAG(IS_ANDROID)

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
const char kMetricsLastSeenPrefix[] = "user_experience_metrics.last_seen.";

// Array of the number of samples in the memory mapped file.
const char kMetricsFileMetricsMetadata[] =
    "user_experience_metrics.file_metrics_metadata";

// The number of times the client has been reset due to cloned install.
const char kClonedResetCount[] = "cloned_install.count";

// The first timestamp when we reset a cloned client’s client id. This is only
// set once. Attached to metrics reports forever thereafter.
const char kFirstClonedResetTimestamp[] = "cloned_install.first_timestamp";

// The last timestamp the client is reset due to cloned install. This will be
// updated every time we reset the client due to cloned install.
const char kLastClonedResetTimestamp[] = "cloned_install.last_timestamp";

// A time stamp at which time the browser was known to be alive. Used to
// evaluate whether the browser crash was due to a whole system crash.
// At minimum this is updated each time the "exited_cleanly" preference is
// modified, but can also be optionally updated on a slow schedule.
const char kStabilityBrowserLastLiveTimeStamp[] =
    "user_experience_metrics.stability.browser_last_live_timestamp";

// Number of times the application exited uncleanly since the last report
// due to a gms core update.
const char kStabilityCrashCountDueToGmsCoreUpdate[] =
    "user_experience_metrics.stability.crash_count_due_to_gms_core_update";

// True if the previous run of the program exited cleanly.
const char kStabilityExitedCleanly[] =
    "user_experience_metrics.stability.exited_cleanly";

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

#if BUILDFLAG(IS_ANDROID)
// Number of times the application was launched since last report. Used on
// Android platforms as WebView may still be interested in this metric.
const char kStabilityLaunchCount[] =
    "user_experience_metrics.stability.launch_count";

// Number of times a page load event occurred since the last report.
const char kStabilityPageLoadCount[] =
    "user_experience_metrics.stability.page_load_count";

// Number of times a renderer process successfully launched since the last
// report. Used on Android platforms as WebView may still be interested in this
// metric.
const char kStabilityRendererLaunchCount[] =
    "user_experience_metrics.stability.renderer_launch_count";
#endif

// Base64 encoded serialized UMA system profile proto from the previous session.
const char kStabilitySavedSystemProfile[] =
    "user_experience_metrics.stability.saved_system_profile";

// SHA-1 hash of the serialized UMA system profile proto (hex encoded).
const char kStabilitySavedSystemProfileHash[] =
    "user_experience_metrics.stability.saved_system_profile_hash";

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

// Dictionary for measuring cellular data used by UKM service during last 7
// days.
const char kUkmCellDataUse[] = "user_experience_metrics.ukm_cell_datause";

// Dictionary for measuring cellular data used by UMA service during last 7
// days.
const char kUmaCellDataUse[] = "user_experience_metrics.uma_cell_datause";

// Dictionary for measuring cellular data used by user including chrome services
// per day.
const char kUserCellDataUse[] = "user_experience_metrics.user_call_datause";

// String for holding user ID associated with the current ongoing UMA
// log. This pref will be used to determine whether to send metrics in case
// of a crash.
const char kMetricsCurrentUserId[] = "metrics.current_user_id";

}  // namespace prefs
}  // namespace metrics
