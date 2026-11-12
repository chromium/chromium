// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/private_insights_service.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/private_insights/fcp_event_publisher.h"
#include "components/private_insights/fcp_files.h"
#include "components/private_insights/fcp_flags.h"
#include "components/private_insights/fcp_http_client.h"
#include "components/private_insights/fcp_log_manager.h"
#include "components/private_insights/fcp_simple_task_environment.h"
#include "components/private_insights/populations/pending_events_batch.h"
#include "components/private_insights/populations/population.h"
#include "components/private_insights/populations/typed_population.h"
#include "components/private_insights/private_insights_features.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"
#include "third_party/federated_compute/src/fcp/client/fl_runner.h"

namespace private_insights {

namespace {

// Histogram name under `kPopulationHistogramPrefix`.
constexpr char kContextualCueEventsHistogramName[] = "ContextualCueEvents";

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
    : local_state_(local_state),
      profile_dir_(profile_dir),
      contextual_cue_events_(base::StrCat(
          {kPopulationHistogramPrefix, kContextualCueEventsHistogramName})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_state_);

  // `kPrivateInsightsUploadTaskPipeline` only selects Contextual Cues' upload
  // path. Read once here so a mid-session flip cannot strand events.
  if (base::FeatureList::IsEnabled(kPrivateInsightsUploadTaskPipeline)) {
    auto contextual_cue_population =
        std::make_unique<TypedPopulation<events::ContextualCueLogEvent>>(
            kContextualCueEventsHistogramName, kFcpPopulationNameContextualCues,
            kMaxContextualCueEvents);
    contextual_cue_population_ = contextual_cue_population.get();
    RegisterPopulation(std::move(contextual_cue_population));
  }

  base::FilePath private_insights_dir =
      profile_dir_.AppendASCII("PrivateInsights");
  base::FilePath base_dir = private_insights_dir.AppendASCII("base_dir");
  base::FilePath cache_dir = private_insights_dir.AppendASCII("cache_dir");

  scoped_refptr<FcpHttpRequestManager> http_request_manager;
  if (url_loader_factory) {
    http_request_manager = base::MakeRefCounted<FcpHttpRequestManager>(
        std::move(url_loader_factory),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  fcp_task_env_ = base::MakeRefCounted<FcpSimpleTaskEnvironment>(
      base_dir.AsUTF8Unsafe(), cache_dir.AsUTF8Unsafe(),
      std::move(http_request_manager));
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
  // End any round in progress so nothing new is uploaded after, e.g., the
  // user turns off metrics reporting. An upload already posted completes.
  upload_round_.clear();
  legacy_upload_in_round_ = false;
}

void PrivateInsightsService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& population : populations_) {
    population->OnShutdown();
  }
  if (!contextual_cue_population_) {
    // Same name `TypedPopulation::OnShutdown()` derives.
    base::UmaHistogramCounts100(
        base::StrCat({kPopulationHistogramPrefix,
                      kContextualCueEventsHistogramName,
                      ".Logging.OnShutdownCount"}),
        static_cast<int>(contextual_cue_events_.size()));
  }
  Stop();
}

void PrivateInsightsService::RegisterPopulation(
    std::unique_ptr<Population> population) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(population);
  populations_.push_back(std::move(population));
}

void PrivateInsightsService::LogContextualCueEvent(
    events::ContextualCueLogEvent event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (contextual_cue_population_) {
    contextual_cue_population_->LogEvent(std::move(event));
    return;
  }
  contextual_cue_events_.Push(std::move(event), kMaxContextualCueEvents.Get());
}

const PrivateInsightsService::ContextualCueEventQueue&
PrivateInsightsService::ContextualCueEventsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return contextual_cue_population_
             ? contextual_cue_population_->queue_for_testing()  // IN-TEST
             : contextual_cue_events_;
}

PrivateInsightsService::UploadTask::UploadTask() = default;
PrivateInsightsService::UploadTask::UploadTask(UploadTask&&) = default;
PrivateInsightsService::UploadTask&
PrivateInsightsService::UploadTask::operator=(UploadTask&&) = default;
PrivateInsightsService::UploadTask::~UploadTask() = default;

PrivateInsightsService::UploadTask PrivateInsightsService::MakeUploadTask(
    Population& population) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UploadTask task;
  task.task_env = fcp_task_env_;
  task.server_uri = kFcpServerUri.Get();
  task.population_name = population.population_name();
  task.trigger_time = base::TimeTicks::Now();
  task.batch = population.TakePendingEvents();
  return task;
}

void PrivateInsightsService::TriggerUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_upload_running_) {
    base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                  TriggerUploadOutcome::kSkippedAlreadyRunning);
    return;
  }

  DCHECK(upload_round_.empty());
  DCHECK(!legacy_upload_in_round_);
  for (size_t i = 0; i < populations_.size(); ++i) {
    if (populations_[i]->pending_event_count() > 0) {
      upload_round_.push_back(i);
    }
  }
  legacy_upload_in_round_ =
      !contextual_cue_population_ && !contextual_cue_events_.empty();
  if (upload_round_.empty() && !legacy_upload_in_round_) {
    base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                  TriggerUploadOutcome::kSkippedNoData);
    return;
  }
  round_task_posted_ = false;
  UploadNextInRound();
}

void PrivateInsightsService::UploadNextInRound() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_upload_running_);
  while (!upload_round_.empty()) {
    Population& population = *populations_[upload_round_.front()];
    upload_round_.pop_front();
    UploadPopulation(population);
    if (is_upload_running_) {
      return;  // Continues from OnUploadTaskComplete().
    }
    // Nothing was posted for this population (no server URI); try the next.
  }
  if (legacy_upload_in_round_) {
    legacy_upload_in_round_ = false;
    UploadLegacyContextualCues();
  }
}

void PrivateInsightsService::UploadLegacyContextualCues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_upload_running_);
  ContextualCueEventQueue::Entries pending_events =
      contextual_cue_events_.TakeAll();
  DCHECK(!pending_events.empty());

  fcp_task_env_->result().Clear();
  SerializeEventsToQueryResult<events::ContextualCueLogEvent>(
      pending_events, &fcp_task_env_->result());

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

  const base::TimeDelta upload_duration =
      base::TimeTicks::Now() - upload_start_time;
  base::UmaHistogramTimes(kUploadTimeHistogram, upload_duration);
  base::UmaHistogramMediumTimes(kUploadTime2Histogram, upload_duration);
  return result;
}

void PrivateInsightsService::UploadPopulation(Population& population) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_upload_running_);

  UploadTask task = MakeUploadTask(population);
  DCHECK_GT(task.batch->size(), 0u);
  if (task.server_uri.empty()) {
    // Misconfiguration; nothing can be uploaded. Record the same federated
    // computation outcome the original path reports from the thread pool, and
    // drop the batch: `task` goes out of scope here without `Requeue()` being
    // called, so the events it took from the population are destroyed with it
    // (the original path doesn't requeue in this case either).
    base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                  TriggerUploadOutcome::kSkippedNoServerUri);
    base::UmaHistogramEnumeration(
        kFederatedComputationOutcomeHistogram,
        FederatedComputationOutcome::kErrorNoServerUri);
    return;
  }

  is_upload_running_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PrivateInsightsService::RunUploadTask, std::move(task)),
      base::BindOnce(&PrivateInsightsService::OnUploadTaskComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  // One `kTaskPosted` per round. The legacy upload records its own, so when
  // it is part of the round it provides the sample.
  if (!round_task_posted_ && !legacy_upload_in_round_) {
    base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                  TriggerUploadOutcome::kTaskPosted);
  }
  round_task_posted_ = true;
}

// static
PrivateInsightsService::UploadTaskResult PrivateInsightsService::RunUploadTask(
    UploadTask task) {
  DCHECK(!task.server_uri.empty());  // Checked in UploadPopulation().
  base::UmaHistogramTimes(kUploadPendingTimeHistogram,
                          base::TimeTicks::Now() - task.trigger_time);

  // Serialize on the thread pool so that the main sequence doesn't pay for
  // serializing potentially many events. `result()` is only ever touched from
  // here while an upload is in flight (uploads are serialized by
  // `is_upload_running_`).
  task.task_env->result().Clear();
  task.batch->SerializeToQueryResult(&task.task_env->result());

  base::TimeTicks upload_start_time = base::TimeTicks::Now();

  FcpEventPublisher fcp_event_publisher;
  FcpFiles fcp_files;
  FcpLogManager fcp_log_manager;
  FcpFlags fcp_flags;

  FederatedComputationParams params{
      .task_env = task.task_env.get(),
      .event_publisher = &fcp_event_publisher,
      .files = &fcp_files,
      .log_manager = &fcp_log_manager,
      .flags = &fcp_flags,
      .federated_service_uri = task.server_uri,
      .api_key = google_apis::GetAPIKey(),
      .population_name = task.population_name,
  };

  FederatedComputationResult result =
      GetRunFederatedComputationFunc().Run(params);

  const base::TimeDelta upload_duration =
      base::TimeTicks::Now() - upload_start_time;
  base::UmaHistogramTimes(kUploadTimeHistogram, upload_duration);
  base::UmaHistogramMediumTimes(kUploadTime2Histogram, upload_duration);
  return {.task = std::move(task), .result = result};
}

// static
PrivateInsightsService::FederatedComputationResult
PrivateInsightsService::RunFederatedComputation(
    const FederatedComputationParams& params) {
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
    ContextualCueEventQueue::Entries pending_events,
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
    // contributing the same data twice, as it would undermine privacy
    // guarantees of FCP.
    contextual_cue_events_.Requeue(std::move(pending_events),
                                   kMaxContextualCueEvents.Get());
  }
}

void PrivateInsightsService::OnUploadTaskComplete(
    UploadTaskResult upload_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_upload_running_ = false;
  const FederatedComputationResult& result = upload_result.result;
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
    // contributing the same data twice, as it would undermine privacy
    // guarantees of FCP.
    upload_result.task.batch->Requeue();
  }
  UploadNextInRound();
}

// static
void PrivateInsightsMetricsServiceAccessor::
    SetForceIsMetricsReportingEnabledPrefLookupForTesting(bool value) {
  SetForceIsMetricsReportingEnabledPrefLookup(value);
}

}  // namespace private_insights
