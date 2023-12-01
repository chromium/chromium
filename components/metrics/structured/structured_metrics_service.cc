// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_service.h"

#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "structured_metrics_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics::structured {

StructuredMetricsService::StructuredMetricsService(
    MetricsServiceClient* client,
    PrefService* local_state,
    std::unique_ptr<StructuredMetricsRecorder> recorder)
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

  // If the StructuredMetricsService is not enabled then return early. The
  // recorder needs to be initialized, but not the reporting service or
  // scheduler.
  if (!structured_metrics_enabled_) {
    return;
  }

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

StructuredMetricsService::~StructuredMetricsService() = default;

void StructuredMetricsService::EnableRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!structured_metrics_enabled_) {
    return;
  }
  if (!initialize_complete_) {
    Initialize();
  }
  recorder_->EnableRecording();
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
  // This is mirroring a check in RotateLogsAndSend.
  if (!recorder_->event_storage()->HasEvents()) {
    return;
  }
  BuildAndStoreLog(reason);
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

  if (!reporting_service_->log_store()->has_unsent_logs()) {
    BuildAndStoreLog(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  }
  reporting_service_->Start();
  scheduler_->RotationFinished();
}

void StructuredMetricsService::BuildAndStoreLog(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  ChromeUserMetricsExtension uma_proto;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InitializeUmaProto(uma_proto);
  recorder_->ProvideEventMetrics(uma_proto);
  const std::string serialized_log = SerializeLog(uma_proto);
  reporting_service_->StoreLog(serialized_log, reason);
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

  SystemProfileProto* system_profile = uma_proto.mutable_system_profile();
  metrics::MetricsLog::RecordCoreSystemProfile(client_, system_profile);
}

// static:
std::string StructuredMetricsService::SerializeLog(
    const ChromeUserMetricsExtension& uma_proto) {
  std::string log_data;
  const bool status = uma_proto.SerializeToString(&log_data);
  DCHECK(status);
  return log_data;
}

void StructuredMetricsService::RegisterPrefs(PrefRegistrySimple* registry) {
  reporting::StructuredMetricsReportingService::RegisterPrefs(registry);
}

UnsentLogStore::UnsentLogStoreLimits
StructuredMetricsService::GetLogStoreLimits() {
  return UnsentLogStore::UnsentLogStoreLimits{
      .min_log_count = static_cast<size_t>(kMinLogQueueCount.Get()),
      .min_queue_size_bytes = static_cast<size_t>(kMinLogQueueSizeBytes.Get()),
      .max_log_size_bytes = static_cast<size_t>(kMaxLogSizeBytes.Get()),
  };
}

void StructuredMetricsService::SetRecorderForTest(
    std::unique_ptr<StructuredMetricsRecorder> recorder) {
  recorder_ = std::move(recorder);
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
    BuildAndStoreLog(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  }
  reporting_service_->Start();
}

}  // namespace metrics::structured
