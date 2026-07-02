// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/private_insights_service.h"

#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/private_metrics/private_insights/fcp_event_publisher.h"
#include "components/metrics/private_metrics/private_insights/fcp_files.h"
#include "components/metrics/private_metrics/private_insights/fcp_flags.h"
#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"
#include "components/metrics/private_metrics/private_insights/fcp_log_manager.h"
#include "components/metrics/private_metrics/private_insights/fcp_simple_task_environment.h"
#include "components/metrics/private_metrics/private_insights/private_insights_features.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"
#include "third_party/federated_compute/src/fcp/client/fl_runner.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#include "third_party/federated_compute/src/fcp/client/tensorflow/tensorflow_runner_factory.h"
#pragma clang diagnostic pop

namespace private_insights {

namespace {

void EnsureTensorflowRunnerRegistered() {
  // This is a workaround for FCP crashing when doing eligibility check without
  // TensorFlow.
  // TODO(b/528431754): Remove this when fixed in FCP.
  static const bool registered = []() {
    fcp::client::GetGlobalTensorflowRunnerFactoryRegistry().Register(
        fcp::client::TensorflowRunnerImplementation::kTensorflowRunnerImpl,
        []() -> std::unique_ptr<fcp::client::TensorflowRunner> {
          // Returning a null is fine here, as we're not using TF. We just want
          // to have a factory that doesn't crash when called.
          return nullptr;
        });
    return true;
  }();
  (void)registered;
}

PrivateInsightsService::FederatedComputationResult
ParseFederatedComputationResult(
    const absl::StatusOr<fcp::client::FLRunnerResult>& result) {  // nocheck
  if (result.ok()) {
    PrivateInsightsService::FederatedComputationOutcome outcome;
    switch (result->contribution_result()) {
      case fcp::client::FLRunnerResult::SUCCESS:
        outcome = PrivateInsightsService::FederatedComputationOutcome::kSuccess;
        break;
      case fcp::client::FLRunnerResult::PARTIAL:
        outcome = PrivateInsightsService::FederatedComputationOutcome::kPartial;
        break;
      case fcp::client::FLRunnerResult::FAIL:
        outcome = PrivateInsightsService::FederatedComputationOutcome::kFailed;
        break;
      default:
        outcome = PrivateInsightsService::FederatedComputationOutcome::kUnknown;
        break;
    }

    return {
        .outcome = outcome,
        .contributed_task_count =
            static_cast<size_t>(result->contributed_task_names().size()),
    };
  }

  const absl::Status& status = result.status();
  const absl::string_view msg = status.message();
  const absl::StatusCode code = status.code();

  if (code == absl::StatusCode::kInvalidArgument &&
      msg.find("The entry point uri is invalid") != absl::string_view::npos) {
    return {
        .outcome = PrivateInsightsService::FederatedComputationOutcome::
            kErrorInvalidEntryPointUri,
        .contributed_task_count = std::nullopt,
    };
  }

  if (msg.find("Failed to read from database") != absl::string_view::npos) {
    return {
        .outcome = PrivateInsightsService::FederatedComputationOutcome::
            kErrorDatabaseReadFailed,
        .contributed_task_count = std::nullopt,
    };
  }

  if (msg.find("Failed to reset the database") != absl::string_view::npos) {
    return {
        .outcome = PrivateInsightsService::FederatedComputationOutcome::
            kErrorDatabaseResetFailed,
        .contributed_task_count = std::nullopt,
    };
  }

  return {
      .outcome =
          PrivateInsightsService::FederatedComputationOutcome::kErrorOther,
      .contributed_task_count = std::nullopt,
  };
}

}  // namespace

PrivateInsightsService::PrivateInsightsService(
    PrefService* local_state,
    const base::FilePath& profile_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : local_state_(local_state), profile_dir_(profile_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_state_);

  base::FilePath private_insights_dir =
      profile_dir_.AppendASCII("PrivateInsights");
  base::FilePath base_dir = private_insights_dir.AppendASCII("base_dir");
  base::FilePath cache_dir = private_insights_dir.AppendASCII("cache_dir");

  std::unique_ptr<FcpHttpRequestManager> http_request_manager;
  if (url_loader_factory) {
    http_request_manager = std::make_unique<FcpHttpRequestManager>(
        std::move(url_loader_factory),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  fcp_task_env_ = base::MakeRefCounted<FcpSimpleTaskEnvironment>(
      base_dir.AsUTF8Unsafe(), cache_dir.AsUTF8Unsafe(),
      std::move(http_request_manager),
      kFcpUseAttestationTransparencyVerifier.Get());
}

PrivateInsightsService::~PrivateInsightsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrivateInsightsService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_registrar_.Init(local_state_);
  pref_registrar_.Add(
      metrics::prefs::kMetricsReportingEnabled,
      base::BindRepeating(&PrivateInsightsService::OnMetricsChoiceChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  OnMetricsChoiceChanged();
}

void PrivateInsightsService::OnMetricsChoiceChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (PrivateInsightsMetricsServiceAccessor::IsMetricsReportingEnabled(
          local_state_)) {
    Start();
  } else {
    Stop();
  }
}

void PrivateInsightsService::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (upload_timer_.IsRunning()) {
    return;
  }
  base::TimeDelta interval = kPrivateInsightsUploadInterval.Get();
  if (interval.is_positive()) {
    upload_timer_.Start(
        FROM_HERE, interval,
        base::BindRepeating(&PrivateInsightsService::TriggerUpload,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void PrivateInsightsService::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  upload_timer_.Stop();
}

void PrivateInsightsService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void PrivateInsightsService::LogContextualCueEvent(
    events::ContextualCueLogEvent event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int max_events = kMaxContextualCueEvents.Get();
  contextual_cue_events_.emplace_back(base::Time::Now(), std::move(event));
  int removed_count = 0;
  while (static_cast<int>(contextual_cue_events_.size()) > max_events) {
    contextual_cue_events_.pop_front();
    removed_count++;
  }
  if (removed_count > 0) {
    base::UmaHistogramCounts100(
        kContextualCueEventsLoggingRemovedCountHistogram, removed_count);
  }
  base::UmaHistogramCounts100(kContextualCueEventsLoggingQueuedCountHistogram,
                              static_cast<int>(contextual_cue_events_.size()));
}

void PrivateInsightsService::TriggerUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_upload_running_) {
    base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                  TriggerUploadOutcome::kSkippedAlreadyRunning);
    return;
  }

  base::circular_deque<ContextualCueEventEntry> pending_events =
      std::move(contextual_cue_events_);
  if (pending_events.empty()) {
    base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                  TriggerUploadOutcome::kSkippedNoData);
    return;
  }

  fcp_task_env_->result().Clear();
  SerializeEventsToQueryResult(pending_events, &fcp_task_env_->result());

  is_upload_running_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PrivateInsightsService::UploadBlocking, fcp_task_env_,
                     base::TimeTicks::Now()),
      base::BindOnce(&PrivateInsightsService::OnUploadComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_events)));

  base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                TriggerUploadOutcome::kTaskPosted);
}

// static
PrivateInsightsService::FederatedComputationResult
PrivateInsightsService::UploadBlocking(
    scoped_refptr<FcpSimpleTaskEnvironment> task_env,
    base::TimeTicks trigger_time) {
  base::UmaHistogramTimes(kUploadPendingTimeHistogram,
                          base::TimeTicks::Now() - trigger_time);
  const std::string server_uri = kFcpServerUri.Get();
  if (server_uri.empty()) {
    return {
        .outcome = FederatedComputationOutcome::kErrorNoServerUri,
        .contributed_task_count = std::nullopt,
    };
  }
  base::TimeTicks upload_start_time = base::TimeTicks::Now();

  FcpEventPublisher fcp_event_publisher;
  FcpFiles fcp_files;
  FcpLogManager fcp_log_manager;
  FcpFlags fcp_flags;

  FederatedComputationParams params{
      .task_env = task_env.get(),
      .event_publisher = &fcp_event_publisher,
      .files = &fcp_files,
      .log_manager = &fcp_log_manager,
      .flags = &fcp_flags,
      .federated_service_uri = server_uri,
      .api_key = google_apis::GetAPIKey(),
      .population_name = kFcpPopulationNameContextualCues.Get(),
  };

  FederatedComputationResult result =
      GetRunFederatedComputationFunc().Run(params);

  base::UmaHistogramTimes(kUploadTimeHistogram,
                          base::TimeTicks::Now() - upload_start_time);
  return result;
}

// static
PrivateInsightsService::FederatedComputationResult
PrivateInsightsService::RunFederatedComputation(
    const FederatedComputationParams& params) {
  EnsureTensorflowRunnerRegistered();
  const std::string client_version =
      base::StrCat({"chrome_v", version_info::GetMajorVersionNumber()});
  absl::StatusOr<fcp::client::FLRunnerResult> statusor =  // nocheck
      fcp::client::RunFederatedComputation(
          /*env_deps=*/params.task_env,
          /*event_publisher=*/params.event_publisher,
          /*files=*/params.files,
          /*log_manager=*/params.log_manager,
          /*flags=*/params.flags,
          /*federated_service_uri=*/params.federated_service_uri,
          /*api_key=*/params.api_key,
          /*test_cert_path=*/"",
          // We don't use session_name per se, so just use population name
          // instead.
          /*session_name=*/params.population_name,
          /*population_name=*/params.population_name,
          /*retry_token=*/"",
          /*client_version=*/client_version,
          /*client_attestation_measurement=*/"");
  return ParseFederatedComputationResult(statusor);
}

// static
PrivateInsightsService::RunFederatedComputationFunc&
PrivateInsightsService::GetRunFederatedComputationFunc() {
  static base::NoDestructor<RunFederatedComputationFunc> func(
      base::BindRepeating(&PrivateInsightsService::RunFederatedComputation));
  return *func;
}

// static
void PrivateInsightsService::SetRunFederatedComputationForTesting(  // IN-TEST
    RunFederatedComputationFunc func) {
  GetRunFederatedComputationFunc() =
      func ? std::move(func)
           : base::BindRepeating(
                 &PrivateInsightsService::RunFederatedComputation);
}

void PrivateInsightsService::OnUploadComplete(
    base::circular_deque<ContextualCueEventEntry> pending_events,
    FederatedComputationResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_upload_running_ = false;
  base::UmaHistogramEnumeration(kFederatedComputationOutcomeHistogram,
                                result.outcome);
  if (result.contributed_task_count.has_value()) {
    base::UmaHistogramCounts100(
        kContributedTaskCountHistogram,
        static_cast<int>(*result.contributed_task_count));
  }

  if (result.outcome == FederatedComputationOutcome::kFailed) {
    // Requeue only when we're sure the contribution failed. If it partially
    // succeeded or we're unsure, we have to drop the data to avoid accidentally
    // contributing the dame data twice, as it would undermine privacy
    // guarantees of FCP.
    RequeueEvents(std::move(pending_events));
  }
}

void PrivateInsightsService::RequeueEvents(
    base::circular_deque<ContextualCueEventEntry> events) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (events.empty()) {
    return;
  }

  int max_events = kMaxContextualCueEvents.Get();
  int requeued_count = 0;
  while (!events.empty() &&
         static_cast<int>(contextual_cue_events_.size()) < max_events) {
    contextual_cue_events_.push_front(std::move(events.back()));
    events.pop_back();
    requeued_count++;
  }
  int dropped_count = static_cast<int>(events.size());

  base::UmaHistogramCounts100(
      kContextualCueEventsRequeuingRequeuedCountHistogram, requeued_count);
  base::UmaHistogramCounts100(
      kContextualCueEventsRequeuingDroppedCountHistogram, dropped_count);
}

// static
void PrivateInsightsService::SerializeEventsToQueryResult(
    const base::circular_deque<ContextualCueEventEntry>& events,
    fcp::client::ExampleQueryResult* query_result) {
  CHECK(query_result);

  query_result->set_result_source(
      fcp::client::ExampleQueryResult::PRIVATE_LOGGER);
  query_result->mutable_stats()->set_output_rows_count(
      static_cast<int32_t>(events.size()));

  auto* proto_values =
      (*query_result->mutable_vector_data()->mutable_vectors())["entry"]
          .mutable_bytes_values();
  auto* time_values =
      (*query_result->mutable_vector_data()
            ->mutable_vectors())["confidential_compute_event_time"]
          .mutable_string_values();
  for (const auto& entry : events) {
    proto_values->add_value(entry.event.SerializeAsString());
    time_values->add_value(base::TimeFormatAsIso8601(entry.timestamp));
  }
}

// static
void PrivateInsightsMetricsServiceAccessor::
    SetForceIsMetricsReportingEnabledPrefLookupForTesting(bool value) {
  SetForceIsMetricsReportingEnabledPrefLookup(value);
}

}  // namespace private_insights
