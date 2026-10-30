// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
#define COMPONENTS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/circular_deque.h"
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
#include "components/private_insights/events/contextual_cue_log_event.pb.h"

class PrefService;

namespace fcp::client {
class ExampleQueryResult;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace private_insights {

class FcpEventPublisher;
class FcpFiles;
class FcpFlags;
class FcpLogManager;
class FcpSimpleTaskEnvironment;

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
inline constexpr char kContextualCueEventsLoggingOnShutdownCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContextualCueEvents.Logging."
    "OnShutdownCount";
inline constexpr char kContextualCueEventsLoggingQueuedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContextualCueEvents.Logging.QueuedCount";
inline constexpr char kContextualCueEventsLoggingRemovedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContextualCueEvents.Logging.RemovedCount";
inline constexpr char kContextualCueEventsRequeuingRequeuedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContextualCueEvents.Requeuing."
    "RequeuedCount";
inline constexpr char kContextualCueEventsRequeuingDroppedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContextualCueEvents.Requeuing.DroppedCount";

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
  template <typename EventProto>
  struct EventEntry {
    base::Time timestamp;
    EventProto event;
  };

  using ContextualCueEventEntry = EventEntry<events::ContextualCueLogEvent>;

  // A snapshot of unsent events for a single population, detached from the
  // queue they were logged to. The batch is created on the main sequence,
  // moved to the thread pool where it serializes itself, and moved back to
  // the main sequence where it can be requeued if the upload failed. It is
  // owned by exactly one sequence at a time, so no synchronization is needed.
  //
  // The interface is type-erased so that the upload machinery
  // (`UploadTask`, `UploadBlocking()`, `OnUploadComplete()`) can be shared
  // across event proto types / populations.
  class PendingEventsBatch {
   public:
    virtual ~PendingEventsBatch() = default;

    // Number of events in the batch.
    virtual size_t size() const = 0;

    // Serializes the events into `query_result`. Runs on the thread pool.
    virtual void SerializeToQueryResult(
        fcp::client::ExampleQueryResult* query_result) const = 0;

    // Puts the events back into the queue they came from. Consumes the
    // events; must be called at most once. Runs on the main sequence.
    virtual void Requeue() = 0;
  };

  // Like `RequeueEvents()` / `SerializeEventsToQueryResult()`, this is only
  // instantiated from the .cc (via `MakeUploadTask()`).
  template <typename EventProto>
  class TypedPendingEventsBatch : public PendingEventsBatch {
   public:
    using Events = base::circular_deque<EventEntry<EventProto>>;
    using RequeueCallback = base::OnceCallback<void(Events)>;

    TypedPendingEventsBatch(Events events, RequeueCallback requeue)
        : events_(std::move(events)), requeue_(std::move(requeue)) {}
    ~TypedPendingEventsBatch() override = default;

    TypedPendingEventsBatch(const TypedPendingEventsBatch&) = delete;
    TypedPendingEventsBatch& operator=(const TypedPendingEventsBatch&) = delete;

    // PendingEventsBatch:
    size_t size() const override { return events_.size(); }
    void SerializeToQueryResult(
        fcp::client::ExampleQueryResult* query_result) const override {
      SerializeEventsToQueryResult<EventProto>(events_, query_result);
    }
    void Requeue() override {
      CHECK(requeue_);
      std::move(requeue_).Run(std::move(events_));
    }

   private:
    Events events_;
    RequeueCallback requeue_;
  };

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
  // `queue` subject to `max_events` and the given requeue histograms (see
  // `RequeueEvents()`). `queue` must be a member of this service, as it is
  // referenced by the requeue callback (guarded by a WeakPtr to `this`).
  //
  // This is a private implementation detail of the service: like the other
  // templates in this class it is defined in the .cc and may only be
  // instantiated from there (see `TriggerUploadTask()`).
  template <typename EventProto>
  UploadTask MakeUploadTask(base::circular_deque<EventEntry<EventProto>>& queue,
                            std::string population_name,
                            int max_events,
                            std::string_view requeued_count_histogram,
                            std::string_view dropped_count_histogram);

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

  void OnUploadComplete(
      base::circular_deque<ContextualCueEventEntry> pending_events,
      FederatedComputationResult result);

  // Prepends `events` back onto `target_queue` (preserving their original
  // order) without letting `target_queue` grow beyond `max_events`. Events
  // that don't fit are dropped. The number of requeued and dropped events are
  // reported to `requeued_count_histogram` and `dropped_count_histogram`
  // respectively.
  //
  // These templates are only instantiated from the .cc, so their definitions
  // live there. Tests exercise them via OnUploadComplete() / TriggerUpload().
  template <typename EventProto>
  void RequeueEvents(base::circular_deque<EventEntry<EventProto>> events,
                     base::circular_deque<EventEntry<EventProto>>& target_queue,
                     int max_events,
                     std::string_view requeued_count_histogram,
                     std::string_view dropped_count_histogram);

  // Serializes `events` into `query_result` in the format expected by the
  // private logger example query.
  template <typename EventProto>
  static void SerializeEventsToQueryResult(
      const base::circular_deque<EventEntry<EventProto>>& events,
      fcp::client::ExampleQueryResult* query_result);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  base::FilePath profile_dir_ GUARDED_BY_CONTEXT(sequence_checker_);
  PrefChangeRegistrar pref_registrar_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool is_upload_running_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::RepeatingTimer upload_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<FcpSimpleTaskEnvironment> fcp_task_env_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::circular_deque<ContextualCueEventEntry> contextual_cue_events_
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
