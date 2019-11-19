// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef COMPONENTS_METRICS_METRICS_LOG_H_
#define COMPONENTS_METRICS_METRICS_LOG_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_service_client.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

class PrefService;

namespace base {
class HistogramFlattener;
class HistogramSamples;
class HistogramSnapshotManager;
}

namespace metrics {

class MetricsProvider;
class MetricsServiceClient;
class DelegatingProvider;

namespace internal {
// Maximum number of events before truncation.
constexpr int kOmniboxEventLimit = 5000;
constexpr int kUserActionEventLimit = 5000;
}  // namespace internal

class MetricsLog {
 public:
  enum LogType {
    INITIAL_STABILITY_LOG,  // The initial log containing stability stats.
    ONGOING_LOG,            // Subsequent logs in a session.
    INDEPENDENT_LOG,        // An independent log from a previous session.
  };

  // Loads "independent" metrics from a metrics provider and executes a
  // callback when complete, which could be immediate or after some
  // execution on a background thread.
  class IndependentMetricsLoader {
   public:
    explicit IndependentMetricsLoader(std::unique_ptr<MetricsLog> log);
    ~IndependentMetricsLoader();

    // Call ProvideIndependentMetrics (which may execute on a background thread)
    // for the |metrics_provider| and execute the |done_callback| when complete
    // with the result (true if successful). Though this can be called multiple
    // times to include data from multiple providers, later calls will override
    // system profile information set by earlier calls.
    void Run(base::OnceCallback<void(bool)> done_callback,
             MetricsProvider* metrics_provider);

    // Extract the filled log. No more Run() operations can be done after this.
    std::unique_ptr<MetricsLog> ReleaseLog();

   private:
    std::unique_ptr<MetricsLog> log_;
    std::unique_ptr<base::HistogramFlattener> flattener_;
    std::unique_ptr<base::HistogramSnapshotManager> snapshot_manager_;

    DISALLOW_COPY_AND_ASSIGN(IndependentMetricsLoader);
  };

  // Creates a new metrics log of the specified type.
  // |client_id| is the identifier for this profile on this installation
  // |session_id| is an integer that's incremented on each application launch
  // |client| is used to interact with the embedder.
  // |local_state| is the PrefService that this instance should use.
  // Note: |this| instance does not take ownership of the |client|, but rather
  // stores a weak pointer to it. The caller should ensure that the |client| is
  // valid for the lifetime of this class.
  MetricsLog(const std::string& client_id,
             int session_id,
             LogType log_type,
             MetricsServiceClient* client);
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

  // Record core profile settings into the SystemProfileProto.
  static void RecordCoreSystemProfile(MetricsServiceClient* client,
                                      SystemProfileProto* system_profile);

  // Record core profile settings into the SystemProfileProto without a client.
  static void RecordCoreSystemProfile(
      const std::string& version,
      metrics::SystemProfileProto::Channel channel,
      const std::string& application_locale,
      const std::string& package_name,
      SystemProfileProto* system_profile);

  // Records a user-initiated action.
  void RecordUserAction(const std::string& key);

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
  // call from prefs. On success, returns true and |app_version| contains the
  // recovered version. Otherwise (if there was no saved environment in prefs
  // or it could not be decoded), returns false and |app_version| is empty.
  bool LoadSavedEnvironmentFromPrefs(PrefService* local_state,
                                     std::string* app_version);

  // Record data from providers about the previous session into the log.
  void RecordPreviousSessionData(DelegatingProvider* delegating_provider);

  // Record data from providers about the current session into the log.
  void RecordCurrentSessionData(DelegatingProvider* delegating_provider,
                                base::TimeDelta incremental_uptime,
                                base::TimeDelta uptime);

  // Stop writing to this record and generate the encoded representation.
  // None of the Record* methods can be called after this is called.
  void CloseLog();

  // Truncate some of the fields within the log that we want to restrict in
  // size due to bandwidth concerns.
  void TruncateEvents();

  // Fills |encoded_log| with the serialized protobuf representation of the
  // record.  Must only be called after CloseLog() has been called.
  void GetEncodedLog(std::string* encoded_log);

  const base::TimeTicks& creation_time() const {
    return creation_time_;
  }

  LogType log_type() const { return log_type_; }

 protected:
  // Exposed for the sake of mocking/accessing in test code.

  ChromeUserMetricsExtension* uma_proto() { return &uma_proto_; }

  // Exposed to allow subclass to access to export the uma_proto. Can be used
  // by external components to export logs to Chrome.
  const ChromeUserMetricsExtension* uma_proto() const {
    return &uma_proto_;
  }

 private:
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
  MetricsServiceClient* const client_;

  // The time when the current log was created.
  const base::TimeTicks creation_time_;

  // True if the environment has already been filled in by a call to
  // RecordEnvironment() or LoadSavedEnvironmentFromPrefs().
  bool has_environment_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLog);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_H_
