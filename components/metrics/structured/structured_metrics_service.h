// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/structured/storage_manager.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "components/metrics/structured/structured_metrics_scheduler.h"
#include "components/metrics/unsent_log_store.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

FORWARD_DECLARE_TEST(StructuredMetricsServiceTest, RotateLogs);

class PrefRegistrySimple;

namespace metrics {
class StructuredMetricsServiceTestBase;
class TestStructuredMetricsServiceDisabled;
class TestStructuredMetricsService;

FORWARD_DECLARE_TEST(TestStructuredMetricsServiceDisabled,
                     ValidStateWhenDisabled);

FORWARD_DECLARE_TEST(TestStructuredMetricsService, CreateLogs);
}  // namespace metrics

namespace metrics::structured {

class OobeStructuredMetricsWatcher;
class StructuredMetricsServiceTest;
class StructuredMetricsMixin;

FORWARD_DECLARE_TEST(StructuredMetricsServiceTest, RotateLogs);

// The Structured Metrics Service is responsible for collecting and uploading
// Structured Metric events.
class StructuredMetricsService final {
 public:
  StructuredMetricsService(MetricsServiceClient* client,
                           PrefService* local_state,
                           std::unique_ptr<StructuredMetricsRecorder> recorder);

  ~StructuredMetricsService();

  StructuredMetricsService(const StructuredMetricsService&) = delete;
  StructuredMetricsService& operator=(StructuredMetricsService&) = delete;

  void EnableRecording();

  void RegisterMetricsProvider(std::unique_ptr<MetricsProvider> provider);
  void DisableRecording();

  void EnableReporting();
  void DisableReporting();

  // Flushes any event currently in the recorder to prefs.
  void Flush(metrics::MetricsLogsEventManager::CreateReason reason);

  // Clears all event and log data.
  void Purge();

  MetricsServiceClient* GetMetricsServiceClient() const;

  bool reporting_active() const {
    return reporting_service_->reporting_active();
  }

  bool recording_enabled() const { return recorder_->recording_enabled(); }

  StructuredMetricsRecorder* recorder() { return recorder_.get(); }

  static void RegisterPrefs(PrefRegistrySimple* registry);

  metrics::LogStore* log_store() { return reporting_service_->log_store(); }

  reporting::StructuredMetricsReportingService* reporting_service() {
    return reporting_service_.get();
  }

 private:
  friend class StructuredMetricsServiceTest;
  friend class StructuredMetricsMixin;
#if BUILDFLAG(IS_CHROMEOS)
  friend class OobeStructuredMetricsWatcher;
#endif
  friend class metrics::StructuredMetricsServiceTestBase;

  FRIEND_TEST_ALL_PREFIXES(metrics::structured::StructuredMetricsServiceTest,
                           RotateLogs);
  FRIEND_TEST_ALL_PREFIXES(metrics::TestStructuredMetricsService, CreateLogs);
  FRIEND_TEST_ALL_PREFIXES(metrics::TestStructuredMetricsServiceDisabled,
                           ValidStateWhenDisabled);

  // Sets the instance of the recorder used for test.
  void SetRecorderForTest(std::unique_ptr<StructuredMetricsRecorder> recorder);

  // Callback function to get the upload interval.
  base::TimeDelta GetUploadTimeInterval();

  // Creates a new log and sends any currently stages logs.
  void RotateLogsAndSend();

  // Populates an UMA proto with data that must be accessed form the UI
  // sequence. A task to collect events is posted which updates the created UMA
  // proto. On Windows, Mac, and Linux logs are built synchronously.
  //
  // Must be called from the UI sequence.
  void CreateLogs(metrics::MetricsLogsEventManager::CreateReason reason,
                  bool notify_scheduler);

  // Adds metadata to the uma proto and stores a temporary log into the log
  // store.
  void StoreLog(metrics::MetricsLogsEventManager::CreateReason reason,
                StructuredDataProto structured_data);

  // Adds metadata to the uma proto, stores a temporary log into the log store,
  // and starts an upload.
  void StoreLogAndStartUpload(
      metrics::MetricsLogsEventManager::CreateReason reason,
      bool notify_scheduler,
      StructuredDataProto structured_data);

  // Starts the initialization process for |this|.
  void Initialize();

  // Fills out the UMA proto to be sent.
  void InitializeUmaProto(ChromeUserMetricsExtension& uma_proto);

  // Triggers an upload of recorded events outside of the normal cadence.
  // This doesn't interfere with the normal cadence.
  void ManualUpload();

  // Queue an upload if there are logs stored in the log store. This is meant to
  // be used to start an upload when the service starts, so we do not have to
  // wait until first upload to send events from the previous session.
  //
  // Reporting is assumed to be enabled by function. Must be checked before
  // called.
  void MaybeStartUpload();

  // Sets callback to be performed after a logs is created and stored. When set
  // uploads will be blocked.
  void SetCreateLogsCallbackInTests(base::OnceClosure callback);

  // Helper function to serialize a ChromeUserMetricsExtension proto.
  static std::string SerializeLog(const ChromeUserMetricsExtension& uma_proto);

  // Retrieves the storage parameters to control the reporting service.
  static UnsentLogStore::UnsentLogStoreLimits GetLogStoreLimits();

  // The time at which the current log was created.
  base::TimeTicks log_creation_time_;

  // Registered metrics providers.
  metrics::DelegatingProvider metrics_providers_;

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

  // Flag to make sure MaybeStartUpload() isn't called twice.
  bool initial_upload_started_ = false;

  // The metrics client |this| is service is associated.
  raw_ptr<MetricsServiceClient> client_;

  // Callback to be performed once a log is created and stored.
  base::OnceClosure create_log_callback_for_tests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StructuredMetricsService> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_
