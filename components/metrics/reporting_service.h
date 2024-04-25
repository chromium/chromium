// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that sends metrics logs to a server.

#ifndef COMPONENTS_METRICS_REPORTING_SERVICE_H_
#define COMPONENTS_METRICS_REPORTING_SERVICE_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/data_use_tracker.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "url/gurl.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

class LogStore;
class MetricsUploadScheduler;
class MetricsServiceClient;

// ReportingService is an abstract class which uploads serialized logs from a
// LogStore to a remote server. A concrete implementation of this class must
// provide the specific LogStore and parameters for the MetricsLogUploader, and
// can also implement hooks to record histograms based on certain events that
// occur while attempting to upload logs.
class ReportingService {
 public:
  // Creates a ReportingService with the given |client|, |local_state|,
  // |max_retransmit_size|, and |logs_event_manager|. Does not take ownership
  // of the parameters; instead it stores a weak pointer to each. Caller should
  // ensure that the parameters are valid for the lifetime of this class.
  // |logs_event_manager| is used to notify observers of log events. Can be set
  // to null if observing the events is not necessary.
  ReportingService(MetricsServiceClient* client,
                   PrefService* local_state,
                   size_t max_retransmit_size,
                   MetricsLogsEventManager* logs_event_manager);

  ReportingService(const ReportingService&) = delete;
  ReportingService& operator=(const ReportingService&) = delete;

  virtual ~ReportingService();

  // Completes setup tasks that can't be done at construction time.
  // Loads persisted logs and creates the MetricsUploadScheduler.
  void Initialize();

  // Starts the metrics reporting system.
  // Should be called when metrics enabled or new logs are created.
  // When the service is already running, this is a safe no-op.
  void Start();

  // Shuts down the metrics system. Should be called at shutdown, or if metrics
  // are turned off.
  void Stop();

  // Enable/disable transmission of accumulated logs and crash reports (dumps).
  // Calling Start() automatically enables reporting, but sending is
  // asyncronous so this can be called immediately after Start() to prevent
  // any uploading.
  void EnableReporting();
  void DisableReporting();

  // True iff reporting is currently enabled.
  bool reporting_active() const;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void SetIsInForegound(bool is_in_foreground) {
    is_in_foreground_ = is_in_foreground;
  }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Registers local state prefs used by this class. This should only be called
  // once.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  MetricsServiceClient* client() const { return client_; }

 private:
  // Retrieves the log store backing this service.
  virtual LogStore* log_store() = 0;

  // Getters for MetricsLogUploader parameters.
  virtual GURL GetUploadUrl() const = 0;
  virtual GURL GetInsecureUploadUrl() const = 0;
  virtual std::string_view upload_mime_type() const = 0;
  virtual MetricsLogUploader::MetricServiceType service_type() const = 0;

  // Methods for recording data to histograms.
  virtual void LogActualUploadInterval(base::TimeDelta interval) {}
  virtual void LogCellularConstraint(bool upload_canceled) {}
  virtual void LogResponseOrErrorCode(int response_code,
                                      int error_code,
                                      bool was_https) {}
  virtual void LogSuccessLogSize(size_t log_size) {}
  virtual void LogSuccessMetadata(const std::string& staged_log) {}
  virtual void LogLargeRejection(size_t log_size) {}

  // If recording is enabled, begins uploading the next completed log from
  // the log manager, staging it if necessary.
  void SendNextLog();

  // Uploads the currently staged log (which must be non-null).
  void SendStagedLog();

  // Called after transmission completes (either successfully or with failure).
  // If |force_discard| is true, discard the log regardless of the response or
  // error code. For example, this is used for builds that do not include any
  // metrics server URLs (no reason to keep re-sending to a non-existent URL).
  void OnLogUploadComplete(int response_code,
                           int error_code,
                           bool was_https,
                           bool force_discard,
                           std::string_view force_discard_reason);

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  const raw_ptr<MetricsServiceClient> client_;

  // Used to flush changes to disk after uploading a log. Weak pointer; must
  // outlive |this| instance.
  const raw_ptr<PrefService> local_state_;

  // Largest log size to attempt to retransmit.
  size_t max_retransmit_size_;

  // Event manager to notify observers of log events.
  const raw_ptr<MetricsLogsEventManager> logs_event_manager_;

  // Indicate whether recording and reporting are currently happening.
  // These should not be set directly, but by calling SetRecording and
  // SetReporting.
  bool reporting_active_;

  // Instance of the helper class for uploading logs.
  std::unique_ptr<MetricsLogUploader> log_uploader_;

  // Whether there is a current log upload in progress.
  bool log_upload_in_progress_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<MetricsUploadScheduler> upload_scheduler_;

  // Pointer used for obtaining data use pref updater callback on above layers.
  std::unique_ptr<DataUseTracker> data_use_tracker_;

  // The tick count of the last time log upload has been finished and null if no
  // upload has been done yet.
  base::TimeTicks last_upload_finish_time_;

  // Info on current reporting state to send along with reports.
  ReportingInfo reporting_info_;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Indicates whether the browser is currently in the foreground. Used to
  // determine whether |local_state_| should be flushed immediately after
  // uploading a log.
  bool is_in_foreground_ = false;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as
  // ReportingService.
  base::WeakPtrFactory<ReportingService> self_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_REPORTING_SERVICE_H_
