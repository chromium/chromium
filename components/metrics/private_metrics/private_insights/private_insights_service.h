// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_

#include <optional>
#include <string>

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
#include "components/metrics/private_metrics/private_insights/events/contextual_cue_log_event.pb.h"
#include "components/prefs/pref_change_registrar.h"

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
inline constexpr char kContributedTaskCountHistogram[] =
    "PrivateMetrics.PrivateInsights.ContributedTaskCount";
inline constexpr char kFederatedComputationOutcomeHistogram[] =
    "PrivateMetrics.PrivateInsights.FederatedComputationOutcome";
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
    kMaxValue = kTaskPosted,
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
  struct ContextualCueEventEntry {
    base::Time timestamp;
    events::ContextualCueLogEvent event;
  };

  void OnMetricsChoiceChanged();

  void TriggerUpload();

  // Runs on a background thread pool sequence (allows blocking).
  static FederatedComputationResult UploadBlocking(
      scoped_refptr<FcpSimpleTaskEnvironment> task_env,
      base::TimeTicks trigger_time);

  static FederatedComputationResult RunFederatedComputation(
      const FederatedComputationParams& params);

  static RunFederatedComputationFunc& GetRunFederatedComputationFunc();

  void OnUploadComplete(
      base::circular_deque<ContextualCueEventEntry> pending_events,
      FederatedComputationResult result);

  void RequeueEvents(base::circular_deque<ContextualCueEventEntry> events);

  static void SerializeEventsToQueryResult(
      const base::circular_deque<ContextualCueEventEntry>& events,
      fcp::client::ExampleQueryResult* query_result);

  raw_ptr<PrefService> local_state_ = nullptr;
  base::FilePath profile_dir_;
  PrefChangeRegistrar pref_registrar_;

  bool is_upload_running_ = false;
  base::RepeatingTimer upload_timer_;

  scoped_refptr<FcpSimpleTaskEnvironment> fcp_task_env_;

  base::circular_deque<ContextualCueEventEntry> contextual_cue_events_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PrivateInsightsService> weak_ptr_factory_{this};

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
  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTriggerUploadTest,
                           HandleEvents);
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
