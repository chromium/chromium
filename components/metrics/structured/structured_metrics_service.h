// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
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
class StructuredMetricsService final : public StorageManager::StorageDelegate {
 public:
  StructuredMetricsService(MetricsServiceClient* client,
                           PrefService* local_state,
                           scoped_refptr<StructuredMetricsRecorder> recorder);

  ~StructuredMetricsService() override;

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

  MetricsServiceClient* GetMetricsServiceClient() const;

  bool reporting_active() const {
    return reporting_service_->reporting_active();
  }

  bool recording_enabled() const { return recorder_->recording_enabled(); }

  StructuredMetricsRecorder* recorder() { return recorder_.get(); }

  static void RegisterPrefs(PrefRegistrySimple* registry);

  metrics::LogStore* log_store() { return reporting_service_->log_store(); }

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
  void SetRecorderForTest(scoped_refptr<StructuredMetricsRecorder> recorder);

  // Callback function to get the upload interval.
  base::TimeDelta GetUploadTimeInterval();

  // Creates a new log and sends any currently stages logs.
  void RotateLogsAndSend();

  // Collects the events from the recorder and builds a new log on a separate
  // task.
  //
  // An upload is triggered once the task is completed.
  void BuildAndStoreLog(metrics::MetricsLogsEventManager::CreateReason reason,
                        bool notify_scheduler);

  // Collects the events from the recorder and builds a new log on the current
  // thread.
  //
  // An upload is triggered after the log has been stored.
  // Used on Windows, Mac, and Linux and during shutdown.
  void BuildAndStoreLogSync(
      metrics::MetricsLogsEventManager::CreateReason reason,
      bool notify_scheduler);

  // Populates an UMA proto with data that must be accessed form the UI
  // sequence. A task to collect events is posted which updates the created UMA
  // proto. On Windows, Mac, and Linux logs are built synchronously.
  //
  // Must be called from the UI sequence.
  void CreateLogs(metrics::MetricsLogsEventManager::CreateReason reason,
                  bool notify_scheduler);

  // Adds metadata to the uma proto, stores a temporary log into the log store,
  // and starts an upload.
  void StoreLogAndStartUpload(
      metrics::MetricsLogsEventManager::CreateReason reason,
      bool notify_scheduler,
      ChromeUserMetricsExtension uma_proto);

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

  // StorageManager::StorageDelegate:
  void OnFlushed(const FlushedKey& key) override;
  void OnDeleted(const FlushedKey& key, DeleteReason reason) override;

  // Helper function to serialize a ChromeUserMetricsExtension proto.
  static std::string SerializeLog(const ChromeUserMetricsExtension& uma_proto);

  // Retrieves the storage parameters to control the reporting service.
  static UnsentLogStore::UnsentLogStoreLimits GetLogStoreLimits();

  // Manages on-device recording of events.
  scoped_refptr<StructuredMetricsRecorder> recorder_;

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

// Access to |recorder_| through |task_runner_| is only needed on Ash Chrome.
// Other platforms can continue to access |recorder_| directly.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // An IO task runner for creating logs.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // A helper class for performing asynchronous IO task on the
  // StructuredMetricsRecorder.
  class ServiceIOHelper {
   public:
    explicit ServiceIOHelper(scoped_refptr<StructuredMetricsRecorder> recorder);

    ~ServiceIOHelper();

    // Reads the events from |recorder_|.
    ChromeUserMetricsExtension ProvideEvents();

   private:
    // Access to the recorder is thead-safe.
    scoped_refptr<StructuredMetricsRecorder> recorder_;
  };

  // Holds a refptr to |recorder_| and provides access through |task_runner_|.
  base::SequenceBound<ServiceIOHelper> io_helper_;
#endif
  base::WeakPtrFactory<StructuredMetricsService> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SERVICE_H_
