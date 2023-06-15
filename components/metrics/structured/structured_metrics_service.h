// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "components/metrics/structured/structured_metrics_scheduler.h"
#include "components/metrics/unsent_log_store.h"

FORWARD_DECLARE_TEST(StructuredMetricsServiceTest, RotateLogs);

class PrefRegistrySimple;

namespace metrics {
class StructuredMetricsServiceTestBase;
class TestStructuredMetricsServiceDisabled;

FORWARD_DECLARE_TEST(TestStructuredMetricsServiceDisabled,
                     ValidStateWhenDisabled);
}  // namespace metrics

namespace metrics::structured {

// The Structured Metrics Service is responsible for collecting and uploading
// Structured Metric events.
class StructuredMetricsService final {
 public:
  StructuredMetricsService(MetricsProvider* system_profile_provider,
                           MetricsServiceClient* client,
                           PrefService* local_state);

  ~StructuredMetricsService();

  StructuredMetricsService(const StructuredMetricsService&) = delete;
  StructuredMetricsService& operator=(StructuredMetricsService&) = delete;

  void EnableRecording();
  void DisableRecording();

  void EnableReporting();
  void DisableReporting();

  // Flushes any event currently in the recorder to prefs.
  void Flush(metrics::MetricsLogsEventManager::CreateReason reason);

  // Clears all event and log data.
  void Purge();

  bool reporting_active() const {
    return reporting_service_->reporting_active();
  }

  bool recording_enabled() const { return recorder_->recording_enabled(); }

  StructuredMetricsRecorder* recorder() { return recorder_.get(); }

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend class StructuredMetricsServiceTest;
  friend class metrics::StructuredMetricsServiceTestBase;

  FRIEND_TEST_ALL_PREFIXES(StructuredMetricsServiceTest, RotateLogs);
  FRIEND_TEST_ALL_PREFIXES(metrics::TestStructuredMetricsServiceDisabled,
                           ValidStateWhenDisabled);

  StructuredMetricsService(MetricsServiceClient* client,
                           PrefService* local_state,
                           std::unique_ptr<StructuredMetricsRecorder> recorder);

  // Callback function to get the upload interval.
  base::TimeDelta GetUploadTimeInterval();

  // Creates a new log and sends any currently stages logs.
  void RotateLogsAndSend();

  // Collects the events from the recorder and builds a new log.
  void BuildAndStoreLog(metrics::MetricsLogsEventManager::CreateReason reason);

  // Starts the initialization process for |this|.
  void Initialize();

  // Fills out the UMA proto to be sent.
  void InitializeUmaProto(ChromeUserMetricsExtension& uma_proto);

  // Helper function to serialize a ChromeUserMetricsExtension proto.
  static std::string SerializeLog(const ChromeUserMetricsExtension& uma_proto);

  // Retrieves the storage parameters to control the reporting service.
  static UnsentLogStore::UnsentLogStoreLimits GetLogStoreLimits();

  // Manages on-device recording of events.
  std::unique_ptr<StructuredMetricsRecorder> recorder_;

  // Service for uploading completed logs.
  std::unique_ptr<reporting::StructuredMetricsReportingService>
      reporting_service_;

  // Schedules when logs will be created.
  std::unique_ptr<StructuredMetricsScheduler> scheduler_;

  // Marks that initialization has completed.
  bool initialize_complete_ = false;

  // Represents if structured metrics and the service is enabled. This isn't
  // to indicate if the service is recording.
  bool structured_metrics_enabled_ = false;

  // The metrics client |this| is service is associated.
  base::raw_ptr<MetricsServiceClient> client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StructuredMetricsService> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_
