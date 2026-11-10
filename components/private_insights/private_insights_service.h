// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
#define COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/private_insights/event_queue.h"
#include "components/private_insights/events/contextual_cue_log_event.pb.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace private_insights {

class FcpEventPublisher;
class FcpFiles;
class FcpFlags;
class FcpLogManager;
class FcpSimpleTaskEnvironment;
class PendingEventsBatch;

class PrivateInsightsService;

class COMPONENT_EXPORT(PRIVATE_INSIGHTS) PrivateInsightsMetricsServiceAccessor
    : public metrics::MetricsServiceAccessor {
 public:
  static void SetForceIsMetricsReportingEnabledPrefLookupForTesting(bool value);

 private:
  friend class PrivateInsightsService;
};

inline constexpr char kTriggerUploadOutcomeHistogram[] =
    "PrivateMetrics.PrivateInsights.TriggerUploadOutcome";
inline constexpr char kUploadPendingTimeHistogram[] =
    "PrivateMetrics.PrivateInsights.Upload.PendingTime";
inline constexpr char kUploadTimeHistogram[] =
    "PrivateMetrics.PrivateInsights.Upload.Time";
inline constexpr char kUploadTime2Histogram[] =
    "PrivateMetrics.PrivateInsights.Upload.Time2";
inline constexpr char kContributedTaskCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContributedTaskCount";
inline constexpr char kFederatedComputationOutcomeHistogram[] =
    "PrivateMetrics.PrivateInsights.FederatedComputationOutcome";
// Prefix of the Contextual Cues `EventQueue` health histograms (see
// `EventQueue` for the suffixes it appends).
inline constexpr char kContextualCueEventsHistogramPrefix[] =
    "PrivateMetrics.PrivateInsights.ContextualCueEvents";
inline constexpr char kContextualCueEventsLoggingOnShutdownCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContextualCueEvents.Logging."
    "OnShutdownCount";

class COMPONENT_EXPORT(PRIVATE_INSIGHTS) PrivateInsightsService
    : public KeyedService {
 public:
  struct FederatedComputationParams {
    raw_ptr<FcpSimpleTaskEnvironment> task_env;
    raw_ptr<FcpEventPublisher> event_publisher;
    raw_ptr<FcpFiles> files;
    raw_ptr<FcpLogManager> log_manager;
    raw_ptr<FcpFlags> flags;
    std::string federated_service_uri;
    std::string api_key;
    std::string session_name;
    std::string population_name;
  };

  // LINT.IfChange(PrivateInsightsFederatedComputationOutcome)
  enum class FederatedComputationOutcome {
    kSuccess = 0,
    kPartial = 1,
    kFailed = 2,
    kUnknown = 3,
    kErrorOther = 4,
    kErrorNoServerUri = 5,
    kErrorInvalidEntryPointUri = 6,
    kErrorDatabaseReadFailed = 7,
    kErrorDatabaseResetFailed = 8,
    kMaxValue = kErrorDatabaseResetFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/private_metrics/enums.xml:PrivateInsightsFederatedComputationOutcome)

  struct FederatedComputationResult {
    FederatedComputationOutcome outcome;
    std::optional<size_t> contributed_task_count;
  };

  using RunFederatedComputationFunc =
      base::RepeatingCallback<FederatedComputationResult(
          const FederatedComputationParams& params)>;

  // LINT.IfChange(PrivateInsightsTriggerUploadOutcome)
  enum class TriggerUploadOutcome {
    kSkippedAlreadyRunning = 0,
    kTaskPosted = 1,
    kSkippedNoData = 2,
    // Only reported by the `UploadTask` pipeline, which validates the server
    // URI before posting to the thread pool.
    kSkippedNoServerUri = 3,
    kMaxValue = kSkippedNoServerUri,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/private_metrics/enums.xml:PrivateInsightsTriggerUploadOutcome)

  PrivateInsightsService(
      PrefService* local_state,
      const base::FilePath& profile_dir,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PrivateInsightsService() override;

  PrivateInsightsService(const PrivateInsightsService&) = delete;
  PrivateInsightsService& operator=(const PrivateInsightsService&) = delete;

  void Init();
  void Start();
  void Stop();

  // KeyedService:
  void Shutdown() override;

  static void SetRunFederatedComputationForTesting(
      RunFederatedComputationFunc func);

  virtual void LogContextualCueEvent(events::ContextualCueLogEvent event);

 private:
  using ContextualCueEventQueue = EventQueue<events::ContextualCueLogEvent>;

  // Everything the thread pool needs to run one federated computation round
  // for a single population. Move-only; it travels main sequence -> thread
  // pool -> main sequence together with its `batch`.
  //
  // Exported separately from the enclosing class because on Windows
  // `__declspec(dllexport)` does not propagate to nested types, and tests in a
  // different module need the out-of-line special members below.
  struct COMPONENT_EXPORT(PRIVATE_INSIGHTS) UploadTask {
    UploadTask();
    UploadTask(UploadTask&&);
    UploadTask& operator=(UploadTask&&);
    UploadTask(const UploadTask&) = delete;
    UploadTask& operator=(const UploadTask&) = delete;
    ~UploadTask();

    scoped_refptr<FcpSimpleTaskEnvironment> task_env;
    // Configuration is snapshotted on the main sequence when the task is
    // created so the upload runs against a consistent view of the Finch
    // params.
    std::string server_uri;
    std::string population_name;
    std::unique_ptr<PendingEventsBatch> batch;
    // When `TriggerUpload()` created this task; used to measure how long the
    // task waited before the thread pool picked it up.
    base::TimeTicks trigger_time;
  };

  // Returned by `UploadBlocking()`. Carries the task back to the main sequence
  // so the batch can be requeued on failure.
  struct UploadTaskResult {
    UploadTask task;
    FederatedComputationResult result;
  };

  void OnMetricsChoiceChanged();

  void TriggerUpload();

  // Detaches all events currently in `queue` into a new `UploadTask` targeting
  // `population_name`. If the upload fails, the events are requeued onto
  // `queue` subject to `max_events` (see `EventQueue::Requeue()`). The requeue
  // callback holds a `WeakPtr` to `queue`, so it is a no-op if the queue has
  // been destroyed by the time the upload completes.
  //
  // This is a private implementation detail of the service: like the other
  // templates in this class it is defined in the .cc and may only be
  // instantiated from there (see `TriggerUploadTask()`).
  template <typename EventProto>
  UploadTask MakeUploadTask(EventQueue<EventProto>& queue,
                            std::string population_name,
                            int max_events);

  // Runs on a background thread pool sequence (allows blocking).
  static FederatedComputationResult UploadBlocking(
      scoped_refptr<FcpSimpleTaskEnvironment> task_env,
      base::TimeTicks trigger_time);

  // `UploadTask` pipeline, used when `kPrivateInsightsUploadTaskPipeline` is
  // enabled. Population agnostic; serializes events on the thread pool.
  // `TriggerUpload()` dispatches here; the original path above is left
  // unchanged until the rollout completes.
  // TODO(b/576348553): Make this the only path and remove the original.
  void TriggerUploadTask();
  // Runs on a background thread pool sequence (allows blocking). Serializes
  // the task's batch into the task environment and runs the federated
  // computation for the task's population.
  static UploadTaskResult RunUploadTask(UploadTask task);
  void OnUploadTaskComplete(UploadTaskResult upload_result);

  static FederatedComputationResult RunFederatedComputation(
      const FederatedComputationParams& params);

  static RunFederatedComputationFunc& GetRunFederatedComputationFunc();

  void OnUploadComplete(ContextualCueEventQueue::Entries pending_events,
                        FederatedComputationResult result);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  base::FilePath profile_dir_ GUARDED_BY_CONTEXT(sequence_checker_);
  PrefChangeRegistrar pref_registrar_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool is_upload_running_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::RepeatingTimer upload_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<FcpSimpleTaskEnvironment> fcp_task_env_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // TODO(b/574637098): Move into a `ContextualCuePopulation` once the
  // population registry lands.
  ContextualCueEventQueue contextual_cue_events_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PrivateInsightsService> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           TriggerUploadSkipsPostingTaskWhenNoData);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           TriggerUploadSkipsPostingTaskWhenAlreadyRunning);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest, MetricsChoiceCoupling);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           MetricsChoiceRespectedOnStartup);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           UploadSkippedWhenServerUriEmpty);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           PopulationNameFinchParam);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest, LogContextualCueEvent);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           SerializeEventsToQueryResult);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest, RequeueEventsEmpty);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           RequeueEventsPrependsRequeuedEvents);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           RequeueEventsExceedsMaxEvents);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           UploadPathSelectedByFlag);
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTriggerUploadTest,
                           HandleEvents);
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
