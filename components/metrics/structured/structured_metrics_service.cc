// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_service.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_scheduler.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics::structured {

#if BUILDFLAG(IS_CHROMEOS_ASH)
StructuredMetricsService::ServiceIOHelper::ServiceIOHelper(
    scoped_refptr<StructuredMetricsRecorder> recorder)
    : recorder_(std::move(recorder)) {}

StructuredMetricsService::ServiceIOHelper::~ServiceIOHelper() = default;

ChromeUserMetricsExtension
StructuredMetricsService::ServiceIOHelper::ProvideEvents() {
  ChromeUserMetricsExtension uma_proto;
  recorder_->ProvideEventMetrics(uma_proto);
  return uma_proto;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

StructuredMetricsService::StructuredMetricsService(
    MetricsServiceClient* client,
    PrefService* local_state,
    scoped_refptr<StructuredMetricsRecorder> recorder)
    : recorder_(std::move(recorder)),
      // This service is only enabled if both structured metrics and the service
      // flags are enabled.
      structured_metrics_enabled_(
          base::FeatureList::IsEnabled(metrics::features::kStructuredMetrics) &&
          base::FeatureList::IsEnabled(kEnabledStructuredMetricsService)),
      client_(client) {
  CHECK(client_);
  CHECK(local_state);
  CHECK(recorder_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       // Blocking because the works being done isn't to expensive.
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  io_helper_.emplace(task_runner_, recorder_);
#endif

  // If the StructuredMetricsService is not enabled then return early. The
  // recorder needs to be initialized, but not the reporting service or
  // scheduler.
  if (!structured_metrics_enabled_) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Because of construction order of the recorder and service, the service
  // needs to be set on the storage manager after it is created.
  if (base::FeatureList::IsEnabled(kEventStorageManager)) {
    StorageManager* storage_manager =
        static_cast<StorageManager*>(recorder_->event_storage());
    storage_manager->set_delegate(this);
  }
#endif

  // Setup the reporting service.
  const UnsentLogStore::UnsentLogStoreLimits storage_limits =
      GetLogStoreLimits();

  reporting_service_ =
      std::make_unique<reporting::StructuredMetricsReportingService>(
          client_, local_state, storage_limits);

  reporting_service_->Initialize();

  // Setup the log rotation scheduler.
  base::RepeatingClosure rotate_callback = base::BindRepeating(
      &StructuredMetricsService::RotateLogsAndSend, weak_factory_.GetWeakPtr());
  base::RepeatingCallback<base::TimeDelta(void)> get_upload_interval_callback =
      base::BindRepeating(&StructuredMetricsService::GetUploadTimeInterval,
                          base::Unretained(this));

  const bool fast_startup_for_test = client->ShouldStartUpFastForTesting();
  scheduler_ = std::make_unique<StructuredMetricsScheduler>(
      rotate_callback, get_upload_interval_callback, fast_startup_for_test);
}

StructuredMetricsService::~StructuredMetricsService() {
  // Will create a new log for all in-memory events.
  // With this, we may be able to add a fast path initialization because flushed
  // events do not need to be loaded.
  if (recorder_ && recorder_->CanProvideMetrics() &&
      recorder_->event_storage()->HasEvents()) {
    Flush(metrics::MetricsLogsEventManager::CreateReason::kServiceShutdown);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Because of construction order of the recorder and service, the delegate
  // must be unset here to avoid dangling pointers.
  if (base::FeatureList::IsEnabled(kEventStorageManager)) {
    StorageManager* storage_manager =
        static_cast<StorageManager*>(recorder_->event_storage());
    storage_manager->unset_delegate(this);
  }
#endif
}

void StructuredMetricsService::EnableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!structured_metrics_enabled_) {
    return;
  }
  if (!initialize_complete_) {
    Initialize();
  }
  recorder_->EnableRecording();

  // Attempt an upload if reporting is also active.
  if (initialize_complete_ && reporting_active()) {
    MaybeStartUpload();
  }
}

void StructuredMetricsService::DisableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!structured_metrics_enabled_) {
    return;
  }
  recorder_->DisableRecording();
}

void StructuredMetricsService::EnableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!structured_metrics_enabled_) {
    return;
  }
  if (!reporting_active()) {
    scheduler_->Start();
  }
  reporting_service_->EnableReporting();

  // Attempt an upload if recording is also enabled.
  if (initialize_complete_ && recording_enabled()) {
    MaybeStartUpload();
  }
}

void StructuredMetricsService::DisableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!structured_metrics_enabled_) {
    return;
  }
  reporting_service_->DisableReporting();
  scheduler_->Stop();
}

void StructuredMetricsService::Flush(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The log should not be built if there aren't any events to log.
  if (!recorder_->event_storage()->HasEvents()) {
    return;
  }

  ChromeUserMetricsExtension uma_proto;
  InitializeUmaProto(uma_proto);
  recorder_->ProvideEventMetrics(uma_proto);
  const std::string serialized_log = SerializeLog(uma_proto);
  reporting_service_->StoreLog(serialized_log, reason);

  reporting_service_->log_store()->TrimAndPersistUnsentLogs(true);
}

void StructuredMetricsService::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!structured_metrics_enabled_) {
    return;
  }
  recorder_->Purge();
  reporting_service_->Purge();
}

base::TimeDelta StructuredMetricsService::GetUploadTimeInterval() {
  return base::Seconds(GetUploadInterval());
}

void StructuredMetricsService::RotateLogsAndSend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Verify that the recorder has been initialized and can be providing metrics.
  // And if it is, then see if there are any events ready to be uploaded.
  if (!recorder_->CanProvideMetrics() ||
      !recorder_->event_storage()->HasEvents()) {
    return;
  }

  // If we do not have any logs then nothing to do.
  if (!reporting_service_->log_store()->has_unsent_logs()) {
    CreateLogs(metrics::MetricsLogsEventManager::CreateReason::kPeriodic,
               /*notify_scheduler=*/true);
    return;
  }

  // If we already have a completed log then we can upload here.
  reporting_service_->Start();
  scheduler_->RotationFinished();
}

void StructuredMetricsService::CreateLogs(
    metrics::MetricsLogsEventManager::CreateReason reason,
    bool notify_scheduler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

// An async version is used on Ash because events could potentially be stored on
// disk and must be accessed from an IO sequence.
// Other platforms (Windows, Mac, and Linux), the events are stored only
// in-memory and thus a blocking function isn't needed.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  BuildAndStoreLog(reason, notify_scheduler);
#else
  BuildAndStoreLogSync(reason, notify_scheduler);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void StructuredMetricsService::BuildAndStoreLog(
    metrics::MetricsLogsEventManager::CreateReason reason,
    bool notify_scheduler) {
  ChromeUserMetricsExtension uma_proto;
  InitializeUmaProto(uma_proto);

  io_helper_.AsyncCall(&ServiceIOHelper::ProvideEvents)
      .Then(base::BindOnce(&StructuredMetricsService::StoreLogAndStartUpload,
                           weak_factory_.GetWeakPtr(), reason,
                           notify_scheduler));
}
#endif

void StructuredMetricsService::BuildAndStoreLogSync(
    metrics::MetricsLogsEventManager::CreateReason reason,
    bool notify_scheduler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ChromeUserMetricsExtension uma_proto;
  InitializeUmaProto(uma_proto);
  recorder_->ProvideEventMetrics(uma_proto);

  StoreLogAndStartUpload(reason, notify_scheduler, std::move(uma_proto));
}

void StructuredMetricsService::StoreLogAndStartUpload(
    metrics::MetricsLogsEventManager::CreateReason reason,
    bool notify_scheduler,
    ChromeUserMetricsExtension uma_proto) {
  // The |uma_proto| is created by |io_helper_|, this adds all additional
  // metadata to the output proto.
  InitializeUmaProto(uma_proto);

  const std::string serialized_log = SerializeLog(uma_proto);
  reporting_service_->StoreLog(serialized_log, reason);

  // If this callback is set, then run it and return.
  // It will only be set from tests where we do not want to upload.
  if (create_log_callback_for_tests_) {
    std::move(create_log_callback_for_tests_).Run();
    return;
  }

  reporting_service_->Start();
  if (notify_scheduler) {
    scheduler_->RotationFinished();
  }
}

void StructuredMetricsService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialize_complete_);

  initialize_complete_ = true;

  // Notifies the scheduler that it is ready to start creating logs.
  scheduler_->InitTaskComplete();
}

void StructuredMetricsService::InitializeUmaProto(
    ChromeUserMetricsExtension& uma_proto) {
  const int32_t product = client_->GetProduct();
  if (product != uma_proto.product()) {
    uma_proto.set_product(product);
  }

  recorder_->ProvideLogMetadata(uma_proto);

  SystemProfileProto* system_profile = uma_proto.mutable_system_profile();
  metrics::MetricsLog::RecordCoreSystemProfile(client_, system_profile);
}

void StructuredMetricsService::RegisterPrefs(PrefRegistrySimple* registry) {
  reporting::StructuredMetricsReportingService::RegisterPrefs(registry);
}

void StructuredMetricsService::SetRecorderForTest(
    scoped_refptr<StructuredMetricsRecorder> recorder) {
  recorder_ = std::move(recorder);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Reset the |io_helper_| with the new recorder.
  io_helper_.emplace(task_runner_, recorder_);
#endif
}

MetricsServiceClient* StructuredMetricsService::GetMetricsServiceClient()
    const {
  return client_;
}

void StructuredMetricsService::ManualUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!recorder_->CanProvideMetrics() ||
      !recorder_->event_storage()->HasEvents()) {
    return;
  }

  if (!reporting_service_->log_store()->has_unsent_logs()) {
    CreateLogs(metrics::MetricsLogsEventManager::CreateReason::kUnknown,
               /*notify_scheduler=*/false);
    return;
  }
  reporting_service_->Start();
}

void StructuredMetricsService::MaybeStartUpload() {
  // We do not have any logs to upload. Nothing to do.
  if (!reporting_service_->log_store()->has_unsent_logs()) {
    return;
  }

  if (initial_upload_started_) {
    return;
  }

  initial_upload_started_ = true;

  // Starts an upload. If a log is not staged the next log will be staged for
  // upload.
  reporting_service_->Start();
}

void StructuredMetricsService::SetCreateLogsCallbackInTests(
    base::OnceClosure callback) {
  create_log_callback_for_tests_ = std::move(callback);
}

void StructuredMetricsService::OnFlushed(const FlushedKey& key) {
  // TODO(b/327269939) Implement telemetry for flushed events.
}

void StructuredMetricsService::OnDeleted(const FlushedKey& key,
                                         DeleteReason reason) {
  // TODO(b/327269939) Implement telemetry for deleted events.
}

// static:
std::string StructuredMetricsService::SerializeLog(
    const ChromeUserMetricsExtension& uma_proto) {
  std::string log_data;
  const bool status = uma_proto.SerializeToString(&log_data);
  DCHECK(status);
  return log_data;
}

// static:
UnsentLogStore::UnsentLogStoreLimits
StructuredMetricsService::GetLogStoreLimits() {
  return UnsentLogStore::UnsentLogStoreLimits{
      .min_log_count = static_cast<size_t>(kMinLogQueueCount.Get()),
      .min_queue_size_bytes = static_cast<size_t>(kMinLogQueueSizeBytes.Get()),
      .max_log_size_bytes = static_cast<size_t>(kMaxLogSizeBytes.Get()),
  };
}

}  // namespace metrics::structured
