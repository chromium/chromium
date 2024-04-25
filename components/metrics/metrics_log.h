// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by the
// MetricsService. This is the unit of data that is sent to the server.

#ifndef COMPONENTS_METRICS_METRICS_LOG_H_
#define COMPONENTS_METRICS_METRICS_LOG_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

class PrefService;

namespace base {
class Clock;
class HistogramSamples;
}  // namespace base

namespace network_time {
class NetworkTimeTracker;
}  // namespace network_time

namespace metrics {

// This SourceType is saved in Local state by unsent_log_store.cc and entries
// should not be renumbered.
enum UkmLogSourceType {
  UKM_ONLY = 0,            // Log contains only UKM data.
  APPKM_ONLY = 1,          // Log contains only AppKM data.
  BOTH_UKM_AND_APPKM = 2,  // Log contains both AppKM and UKM data.
};

// Holds optional metadata associated with a log to be stored.
struct LogMetadata {
  LogMetadata();
  LogMetadata(std::optional<base::HistogramBase::Count> samples_count,
              std::optional<uint64_t> user_id,
              std::optional<UkmLogSourceType> log_source_type);
  LogMetadata(const LogMetadata& other);
  ~LogMetadata();

  // Adds |sample_count| to |samples_count|. If |samples_count| is empty, then
  // |sample_count| will populate |samples_count|.
  void AddSampleCount(base::HistogramBase::Count sample_count);

  // The total number of samples in this log if applicable.
  std::optional<base::HistogramBase::Count> samples_count;

  // User id associated with the log.
  std::optional<uint64_t> user_id;

  // For UKM logs, indicates the type of data.
  std::optional<UkmLogSourceType> log_source_type;
};

class MetricsServiceClient;
class DelegatingProvider;

namespace internal {
// Maximum number of events before truncation.
constexpr int kOmniboxEventLimit = 5000;
constexpr int kUserActionEventLimit = 5000;

SystemProfileProto::InstallerPackage ToInstallerPackage(
    std::string_view installer_package_name);
}  // namespace internal

class MetricsLog {
 public:
  enum LogType {
    INITIAL_STABILITY_LOG,  // The initial log containing stability stats.
    ONGOING_LOG,            // Subsequent logs in a session.
    INDEPENDENT_LOG,        // An independent log from a previous session.
  };

  // Creates a new metrics log of the specified type.
  // |client_id| is the identifier for this profile on this installation
  // |session_id| is an integer that's incremented on each application launch
  // |client| is used to interact with the embedder.
  // Note: |this| instance does not take ownership of the |client|, but rather
  // stores a weak pointer to it. The caller should ensure that the |client| is
  // valid for the lifetime of this class.
  MetricsLog(const std::string& client_id,
             int session_id,
             LogType log_type,
             MetricsServiceClient* client);
  // As above, with a |clock| and |network_clock| to use to vend Now() calls. As
  // with |client|, the caller must ensure both remain valid for the lifetime of
  // this class.
  MetricsLog(const std::string& client_id,
             int session_id,
             LogType log_type,
             base::Clock* clock,
             const network_time::NetworkTimeTracker* network_clock,
             MetricsServiceClient* client);

  MetricsLog(const MetricsLog&) = delete;
  MetricsLog& operator=(const MetricsLog&) = delete;
  virtual ~MetricsLog();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Computes the MD5 hash of the given string, and returns the first 8 bytes of
  // the hash.
  static uint64_t Hash(const std::string& value);

  // Get the GMT buildtime for the current binary, expressed in seconds since
  // January 1, 1970 GMT.
  // The value is used to identify when a new build is run, so that previous
  // reliability stats, from other builds, can be abandoned.
  static int64_t GetBuildTime();

  // Convenience function to return the current time at a resolution in seconds.
  // This wraps base::TimeTicks, and hence provides an abstract time that is
  // always incrementing for use in measuring time durations.
  static int64_t GetCurrentTime();

  // Records core profile settings into the SystemProfileProto.
  static void RecordCoreSystemProfile(MetricsServiceClient* client,
                                      SystemProfileProto* system_profile);

  // Records core profile settings into the SystemProfileProto without a client.
  static void RecordCoreSystemProfile(
      const std::string& version,
      metrics::SystemProfileProto::Channel channel,
      bool is_extended_stable_channel,
      const std::string& application_locale,
      const std::string& package_name,
      SystemProfileProto* system_profile);

  // Assign a unique finalized record id to this log.
  void AssignFinalizedRecordId(PrefService* local_state);

  // Assign a unique record id to this log.
  void AssignRecordId(PrefService* local_state);

  // Records a user-initiated action.
  void RecordUserAction(const std::string& key, base::TimeTicks action_time);

  // Record any changes in a given histogram for transmission.
  void RecordHistogramDelta(const std::string& histogram_name,
                            const base::HistogramSamples& snapshot);

  // TODO(rkaplow): I think this can be a little refactored as it currently
  // records a pretty arbitrary set of things.
  // Records the current operating environment, including metrics provided by
  // the specified |delegating_provider|. The current environment is
  // returned as a SystemProfileProto.
  const SystemProfileProto& RecordEnvironment(
      DelegatingProvider* delegating_provider);

  // Loads the environment proto that was saved by the last RecordEnvironment()
  // call from prefs. On success, returns true. Otherwise, (if there was no
  // saved environment in prefs or it could not be decoded), returns false.
  bool LoadSavedEnvironmentFromPrefs(PrefService* local_state);

  // Populates the log with data about the previous session.
  // |delegating_provider| forwards the call to provide data to registered
  // MetricsProviders. |local_state| is used to schedule a write because a side
  // effect of providing some data is updating Local State prefs.
  void RecordPreviousSessionData(DelegatingProvider* delegating_provider,
                                 PrefService* local_state);

  // Populates the log with data about the current session. The uptimes are used
  // to populate the log with info about how long Chrome has been running.
  // |delegating_provider| forwards the call to provide data to registered
  // MetricsProviders. |local_state| is used to schedule a write because a side
  // effect of providing some data is updating Local State prefs.
  void RecordCurrentSessionData(base::TimeDelta incremental_uptime,
                                base::TimeDelta uptime,
                                DelegatingProvider* delegating_provider,
                                PrefService* local_state);

  // Returns the current time using |network_clock_| if non-null (falls back to
  // |clock_| otherwise). If |record_time_zone| is true, the returned time will
  // also be populated with the time zone. Must be called on the main thread.
  ChromeUserMetricsExtension::RealLocalTime GetCurrentClockTime(
      bool record_time_zone);

  // Finalizes the log. Calling this function will make a call to CloseLog().
  // |truncate_events| determines whether user action and omnibox data within
  // the log should be trimmed/truncated (for bandwidth concerns).
  // |current_app_version| is the current version of the application, and is
  // used to determine whether the log data was obtained in a previous version.
  // |close_time| is roughly the current time -- it is provided as a param
  // since computing the current time can sometimes only be done on the main
  // thread, and this method may be called on a background thread. The
  // serialized proto of the finalized log will be written to |encoded_log|.
  void FinalizeLog(
      bool truncate_events,
      const std::string& current_app_version,
      std::optional<ChromeUserMetricsExtension::RealLocalTime> close_time,
      std::string* encoded_log);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Assigns a user ID to the log. This should be called immediately after
  // consotruction if it should be applied.
  void SetUserId(const std::string& user_id);
#endif

  LogType log_type() const { return log_type_; }

  const LogMetadata& log_metadata() const { return log_metadata_; }

  ChromeUserMetricsExtension* uma_proto() { return &uma_proto_; }

  const ChromeUserMetricsExtension* uma_proto() const { return &uma_proto_; }

 private:
  // Stop writing to this record. None of the Record* methods can be called
  // after this is called.
  void CloseLog();

  // Records the log_written_by_app_version system_profile field if the
  // |current_version| is different from the system_profile's app_version.
  void RecordLogWrittenByAppVersionIfNeeded(const std::string& current_version);

  // Truncate some of the fields within the log that we want to restrict in
  // size due to bandwidth concerns.
  void TruncateEvents();

  // Write the default state of the enable metrics checkbox.
  void WriteMetricsEnableDefault(EnableMetricsDefault metrics_default,
                                 SystemProfileProto* system_profile);

  // Within the stability group, write attributes that need to be updated asap
  // and can't be delayed until the user decides to restart chromium.
  // Delaying these stats would bias metrics away from happy long lived
  // chromium processes (ones that don't crash, and keep on running).
  void WriteRealtimeStabilityAttributes(base::TimeDelta incremental_uptime,
                                        base::TimeDelta uptime);

  // closed_ is true when record has been packed up for sending, and should
  // no longer be written to.  It is only used for sanity checking.
  bool closed_;

  // The type of the log, i.e. initial or ongoing.
  const LogType log_type_;

  // Stores the protocol buffer representation for this log.
  ChromeUserMetricsExtension uma_proto_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  const raw_ptr<MetricsServiceClient> client_;

  // The time when the current log was created.
  const base::TimeTicks creation_time_;

  // True if the environment has already been filled in by a call to
  // RecordEnvironment() or LoadSavedEnvironmentFromPrefs().
  bool has_environment_;

  // Optional metadata associated with the log.
  LogMetadata log_metadata_;

  // The clock used to vend Time::Now().  Note that this is not used for the
  // static function MetricsLog::GetCurrentTime(). Can be overridden for tests.
  raw_ptr<base::Clock> clock_;

  // The NetworkTimeTracker used to provide higher-quality wall clock times than
  // |clock_| (when available). Can be overridden for tests.
  raw_ptr<const network_time::NetworkTimeTracker> network_clock_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_H_
