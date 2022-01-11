// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/optimization_guide/core/hint_cache.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/hints_fetcher_factory.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/insertion_ordered_set.h"
#include "components/optimization_guide/core/optimization_filter.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/core/tab_url_provider.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

// The component version used with a manual config. This ensures that any hint
// component received from the Optimization Hints component on a subsequent
// startup will have a newer version than it.
constexpr char kManualConfigComponentVersion[] = "0.0.0";

// Provides a random time delta in seconds between |kFetchRandomMinDelay| and
// |kFetchRandomMaxDelay|.
base::TimeDelta RandomFetchDelay() {
  return base::Seconds(base::RandInt(
      optimization_guide::features::ActiveTabsHintsFetchRandomMinDelaySecs(),
      optimization_guide::features::ActiveTabsHintsFetchRandomMaxDelaySecs()));
}

void MaybeRunUpdateClosure(base::OnceClosure update_closure) {
  if (update_closure)
    std::move(update_closure).Run();
}

// Returns whether the particular component version can be processed, and if it
// can be, locks the semaphore (in the form of a pref) to signal that the
// processing of this particular version has started.
bool CanProcessComponentVersion(
    PrefService* pref_service,
    const base::Version& version,
    optimization_guide::ProcessHintsComponentResult* out_result) {
  DCHECK(version.IsValid());
  DCHECK(out_result);

  const std::string previous_attempted_version_string = pref_service->GetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion);
  if (!previous_attempted_version_string.empty()) {
    const base::Version previous_attempted_version =
        base::Version(previous_attempted_version_string);
    if (!previous_attempted_version.IsValid()) {
      DLOG(ERROR) << "Bad contents in hints processing pref";
      // Clear pref for fresh start next time.
      pref_service->ClearPref(
          optimization_guide::prefs::kPendingHintsProcessingVersion);
      *out_result = optimization_guide::ProcessHintsComponentResult::
          kFailedPreviouslyAttemptedVersionInvalid;
      return false;
    }
    if (previous_attempted_version.CompareTo(version) == 0) {
      *out_result = optimization_guide::ProcessHintsComponentResult::
          kFailedFinishProcessing;
      // Previously attempted same version without completion.
      return false;
    }
  }

  // Write config version to pref.
  pref_service->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion,
      version.GetString());
  return true;
}

// Returns whether |optimization_type| is allowlisted by |optimizations|. If
// it is allowlisted, this will return true and |optimization_metadata| will be
// populated with the metadata provided by the hint, if applicable. If
// |page_hint| is not provided or |optimization_type| is not allowlisted, this
// will return false.
bool IsOptimizationTypeAllowed(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::Optimization>& optimizations,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  for (const auto& optimization : optimizations) {
    if (optimization_type != optimization.optimization_type())
      continue;

    // We found an optimization that can be applied. Populate optimization
    // metadata if applicable and return.
    if (optimization_metadata) {
      switch (optimization.metadata_case()) {
        case optimization_guide::proto::Optimization::kPerformanceHintsMetadata:
          optimization_metadata->set_performance_hints_metadata(
              optimization.performance_hints_metadata());
          break;
        case optimization_guide::proto::Optimization::kPublicImageMetadata:
          optimization_metadata->set_public_image_metadata(
              optimization.public_image_metadata());
          break;
        case optimization_guide::proto::Optimization::kLoadingPredictorMetadata:
          optimization_metadata->set_loading_predictor_metadata(
              optimization.loading_predictor_metadata());
          break;
        case optimization_guide::proto::Optimization::kAnyMetadata:
          optimization_metadata->set_any_metadata(optimization.any_metadata());
          break;
        case optimization_guide::proto::Optimization::METADATA_NOT_SET:
          // Some optimization types do not have metadata, make sure we do not
          // DCHECK.
          break;
      }
    }
    return true;
  }

  return false;
}

// Util class for recording whether a hints fetch race against the current
// navigation was attempted. The result is recorded when it goes out of scope
// and its destructor is called.
class ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder {
 public:
  explicit ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder(
      OptimizationGuideNavigationData* navigation_data)
      : race_attempt_status_(
            optimization_guide::RaceNavigationFetchAttemptStatus::kUnknown),
        navigation_data_(navigation_data) {}

  ~ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder() {
    DCHECK_NE(race_attempt_status_,
              optimization_guide::RaceNavigationFetchAttemptStatus::kUnknown);
    DCHECK_NE(
        race_attempt_status_,
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        race_attempt_status_);
    if (navigation_data_)
      navigation_data_->set_hints_fetch_attempt_status(race_attempt_status_);
  }

  void set_race_attempt_status(
      optimization_guide::RaceNavigationFetchAttemptStatus
          race_attempt_status) {
    race_attempt_status_ = race_attempt_status;
  }

 private:
  optimization_guide::RaceNavigationFetchAttemptStatus race_attempt_status_;
  OptimizationGuideNavigationData* navigation_data_;
};

// Returns true if the optimization type should be ignored when is newly
// registered as the optimization type is likely launched.
bool ShouldIgnoreNewlyRegisteredOptimizationType(
    optimization_guide::proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case optimization_guide::proto::NOSCRIPT:
    case optimization_guide::proto::RESOURCE_LOADING:
    case optimization_guide::proto::LITE_PAGE_REDIRECT:
    case optimization_guide::proto::DEFER_ALL_SCRIPT:
      return true;
    default:
      return false;
  }
}

class ScopedCanApplyOptimizationLogger {
 public:
  ScopedCanApplyOptimizationLogger(
      optimization_guide::proto::OptimizationType opt_type,
      GURL url)
      : decision_(optimization_guide::OptimizationGuideDecision::kUnknown),
        type_decision_(optimization_guide::OptimizationTypeDecision::kUnknown),
        opt_type_(opt_type),
        has_metadata_(false),
        url_(url) {}

  ~ScopedCanApplyOptimizationLogger() {
    if (!optimization_guide::switches::IsDebugLogsEnabled())
      return;
    DCHECK_NE(type_decision_,
              optimization_guide::OptimizationTypeDecision::kUnknown);
    DVLOG(0) << "OptimizationGuide: CanApplyOptimization: "
             << optimization_guide::GetStringNameForOptimizationType(opt_type_)
             << "\nqueried on: " << url_ << "\nDecision: "
             << GetStringForOptimizationGuideDecision(decision_)
             << "\nTypeDecision: " << static_cast<int>(type_decision_)
             << "\nHas Metadata: " << has_metadata_;
  }

  void set_has_metadata() { has_metadata_ = true; }

  void set_type_decision(
      optimization_guide::OptimizationTypeDecision type_decision) {
    type_decision_ = type_decision;
    decision_ =
        HintsManager::GetOptimizationGuideDecisionFromOptimizationTypeDecision(
            type_decision_);
  }

 private:
  optimization_guide::OptimizationGuideDecision decision_;
  optimization_guide::OptimizationTypeDecision type_decision_;
  optimization_guide::proto::OptimizationType opt_type_;
  bool has_metadata_;
  GURL url_;
};

// Reads component file and parses it into a Configuration proto. Should not be
// called on the UI thread.
std::unique_ptr<optimization_guide::proto::Configuration> ReadComponentFile(
    const optimization_guide::HintsComponentInfo& info) {
  optimization_guide::ProcessHintsComponentResult out_result;
  std::unique_ptr<optimization_guide::proto::Configuration> config =
      optimization_guide::ProcessHintsComponent(info, &out_result);
  if (!config) {
    optimization_guide::RecordProcessHintsComponentResult(out_result);
    return nullptr;
  }

  // Do not record the process hints component result for success cases until
  // we processed all of the hints and filters in it.
  return config;
}

}  // namespace

HintsManager::HintsManager(
    bool is_off_the_record,
    const std::string& application_locale,
    PrefService* pref_service,
    base::WeakPtr<OptimizationGuideStore> hint_store,
    TopHostProvider* top_host_provider,
    TabUrlProvider* tab_url_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<optimization_guide::PushNotificationManager>
        push_notification_manager)
    : is_off_the_record_(is_off_the_record),
      application_locale_(application_locale),
      pref_service_(pref_service),
      hint_cache_(std::make_unique<optimization_guide::HintCache>(
          hint_store,
          optimization_guide::features::MaxHostKeyedHintCacheSize())),
      page_navigation_hints_fetchers_(
          optimization_guide::features::MaxConcurrentPageNavigationFetches()),
      hints_fetcher_factory_(
          std::make_unique<optimization_guide::HintsFetcherFactory>(
              url_loader_factory,
              optimization_guide::features::
                  GetOptimizationGuideServiceGetHintsURL(),
              pref_service,
              network_connection_tracker)),
      top_host_provider_(top_host_provider),
      tab_url_provider_(tab_url_provider),
      push_notification_manager_(std::move(push_notification_manager)),
      clock_(base::DefaultClock::GetInstance()),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  if (push_notification_manager_)
    push_notification_manager_->SetDelegate(this);

  hint_cache_->Initialize(optimization_guide::switches::
                              ShouldPurgeOptimizationGuideStoreOnStartup(),
                          base::BindOnce(&HintsManager::OnHintCacheInitialized,
                                         weak_ptr_factory_.GetWeakPtr()));
}

HintsManager::~HintsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HintsManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->RemoveObserver(this);

  base::UmaHistogramBoolean("OptimizationGuide.ProcessingComponentAtShutdown",
                            is_processing_component_);
  if (is_processing_component_) {
    // If we are currently processing the component and we are asked to shut
    // down, we should clear the pref since the function to clear the pref will
    // not run after shut down and we will think that we failed to process the
    // component due to a crash.
    pref_service_->ClearPref(
        optimization_guide::prefs::kPendingHintsProcessingVersion);
  }
}

// static
optimization_guide::OptimizationGuideDecision
HintsManager::GetOptimizationGuideDecisionFromOptimizationTypeDecision(
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  switch (optimization_type_decision) {
    case optimization_guide::OptimizationTypeDecision::
        kAllowedByOptimizationFilter:
    case optimization_guide::OptimizationTypeDecision::kAllowedByHint:
      return optimization_guide::OptimizationGuideDecision::kTrue;
    case optimization_guide::OptimizationTypeDecision::kUnknown:
    case optimization_guide::OptimizationTypeDecision::
        kHadOptimizationFilterButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHadHintButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHintFetchStartedButNotAvailableInTime:
    case optimization_guide::OptimizationTypeDecision::kDeciderNotInitialized:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
    case optimization_guide::OptimizationTypeDecision::kNotAllowedByHint:
    case optimization_guide::OptimizationTypeDecision::kNoMatchingPageHint:
    case optimization_guide::OptimizationTypeDecision::kNoHintAvailable:
    case optimization_guide::OptimizationTypeDecision::
        kNotAllowedByOptimizationFilter:
      return optimization_guide::OptimizationGuideDecision::kFalse;
  }
}

void HintsManager::OnHintsComponentAvailable(
    const optimization_guide::HintsComponentInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check for if hint component is disabled. This check is needed because the
  // optimization guide still registers with the service as an observer for
  // components as a signal during testing.
  if (optimization_guide::switches::IsHintComponentProcessingDisabled()) {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  optimization_guide::ProcessHintsComponentResult out_result;
  if (!CanProcessComponentVersion(pref_service_, info.version, &out_result)) {
    optimization_guide::RecordProcessHintsComponentResult(out_result);
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  std::unique_ptr<optimization_guide::StoreUpdateData> update_data =
      is_off_the_record_
          ? nullptr
          : hint_cache_->MaybeCreateUpdateDataForComponentHints(info.version);

  // Processes the hints from the newly available component on a background
  // thread, providing a StoreUpdateData for component update from the hint
  // cache, so that each hint within the component can be moved into it. In the
  // case where the component's version is not newer than the optimization guide
  // store's component version, StoreUpdateData will be a nullptr and hint
  // processing will be skipped.
  // base::Unretained(this) is safe since |this| owns |background_task_runner_|
  // and the callback will be canceled if destroyed.
  is_processing_component_ = true;
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadComponentFile, info),
      base::BindOnce(&HintsManager::UpdateComponentHints,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_update_closure_), std::move(update_data)));

  // Only replace hints component info if it is not the same - otherwise we will
  // destruct the object and it will be invalid later.
  if (!hints_component_info_ ||
      hints_component_info_->version.CompareTo(info.version) != 0) {
    hints_component_info_.emplace(info.version, info.path);
  }
}

void HintsManager::ProcessOptimizationFilters(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::OptimizationFilter>&
        allowlist_optimization_filters,
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::OptimizationFilter>&
        blocklist_optimization_filters) {
  optimization_types_with_filter_.clear();
  allowlist_optimization_filters_.clear();
  blocklist_optimization_filters_.clear();
  ProcessOptimizationFilterSet(allowlist_optimization_filters,
                               /*is_allowlist=*/true);
  ProcessOptimizationFilterSet(blocklist_optimization_filters,
                               /*is_allowlist=*/false);
}

void HintsManager::ProcessOptimizationFilterSet(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::OptimizationFilter>& filters,
    bool is_allowlist) {
  for (const auto& filter : filters) {
    if (filter.optimization_type() !=
        optimization_guide::proto::TYPE_UNSPECIFIED) {
      optimization_types_with_filter_.insert(filter.optimization_type());
    }

    // Do not put anything in memory that we don't have registered.
    if (registered_optimization_types_.find(filter.optimization_type()) ==
        registered_optimization_types_.end()) {
      continue;
    }

    optimization_guide::RecordOptimizationFilterStatus(
        filter.optimization_type(),
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig);

    // Do not parse duplicate optimization filters.
    if (allowlist_optimization_filters_.find(filter.optimization_type()) !=
            allowlist_optimization_filters_.end() ||
        blocklist_optimization_filters_.find(filter.optimization_type()) !=
            blocklist_optimization_filters_.end()) {
      optimization_guide::RecordOptimizationFilterStatus(
          filter.optimization_type(),
          optimization_guide::OptimizationFilterStatus::
              kFailedServerFilterDuplicateConfig);
      continue;
    }

    // Parse optimization filter.
    optimization_guide::OptimizationFilterStatus status;
    std::unique_ptr<optimization_guide::OptimizationFilter>
        optimization_filter =
            optimization_guide::ProcessOptimizationFilter(filter, &status);
    if (optimization_filter) {
      if (is_allowlist) {
        allowlist_optimization_filters_.insert(
            {filter.optimization_type(), std::move(optimization_filter)});
      } else {
        blocklist_optimization_filters_.insert(
            {filter.optimization_type(), std::move(optimization_filter)});
      }
    }
    optimization_guide::RecordOptimizationFilterStatus(
        filter.optimization_type(), status);
  }
}

void HintsManager::OnHintCacheInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (push_notification_manager_) {
    push_notification_manager_->OnDelegateReady();
  }

  // Check if there is a valid hint proto given on the command line first. We
  // don't normally expect one, but if one is provided then use that and do not
  // register as an observer as the opt_guide service.
  std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
      optimization_guide::switches::ParseComponentConfigFromCommandLine();
  if (manual_config) {
    std::unique_ptr<optimization_guide::StoreUpdateData> update_data =
        is_off_the_record_
            ? nullptr
            : hint_cache_->MaybeCreateUpdateDataForComponentHints(
                  base::Version(kManualConfigComponentVersion));
    // Allow |UpdateComponentHints| to block startup so that the first
    // navigation gets the hints when a command line hint proto is provided.
    UpdateComponentHints(base::DoNothing(), std::move(update_data),
                         std::move(manual_config));
  }

  // If the store is available, clear all hint state so newly registered types
  // can have their hints immediately included in hint fetches.
  if (hint_cache_->IsHintStoreAvailable() && should_clear_hints_for_new_type_) {
    ClearHostKeyedHints();
    should_clear_hints_for_new_type_ = false;
  }

  // Register as an observer regardless of hint proto override usage. This is
  // needed as a signal during testing.
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->AddObserver(this);
}

void HintsManager::UpdateComponentHints(
    base::OnceClosure update_closure,
    std::unique_ptr<optimization_guide::StoreUpdateData> update_data,
    std::unique_ptr<optimization_guide::proto::Configuration> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we get here, the component file has been processed correctly and did not
  // crash the device.
  is_processing_component_ = false;
  pref_service_->ClearPref(
      optimization_guide::prefs::kPendingHintsProcessingVersion);

  if (!config) {
    MaybeRunUpdateClosure(std::move(update_closure));
    return;
  }

  ProcessOptimizationFilters(config->optimization_allowlists(),
                             config->optimization_blocklists());

  // Don't store hints in the store if it's off the record.
  if (update_data && !is_off_the_record_) {
    bool did_process_hints = hint_cache_->ProcessAndCacheHints(
        config->mutable_hints(), update_data.get());
    optimization_guide::RecordProcessHintsComponentResult(
        did_process_hints
            ? optimization_guide::ProcessHintsComponentResult::kSuccess
            : optimization_guide::ProcessHintsComponentResult::
                  kProcessedNoHints);
  } else {
    optimization_guide::RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints);
  }

  if (update_data) {
    hint_cache_->UpdateComponentHints(
        std::move(update_data),
        base::BindOnce(&HintsManager::OnComponentHintsUpdated,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(update_closure),
                       /* hints_updated=*/true));
  } else {
    OnComponentHintsUpdated(std::move(update_closure), /*hints_updated=*/false);
  }
}

void HintsManager::OnComponentHintsUpdated(base::OnceClosure update_closure,
                                           bool hints_updated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      optimization_guide::kComponentHintsUpdatedResultHistogramString,
      hints_updated);

  // Initiate the hints fetch scheduling if deferred startup handling is not
  // enabled. Otherwise OnDeferredStartup() will iniitate it.
  if (!features::ShouldDeferStartupActiveTabsHintsFetch())
    InitiateHintsFetchScheduling();
  MaybeRunUpdateClosure(std::move(update_closure));
}

void HintsManager::InitiateHintsFetchScheduling() {
  if (optimization_guide::features::
          ShouldBatchUpdateHintsForActiveTabsAndTopHosts()) {
    SetLastHintsFetchAttemptTime(clock_->Now());

    if (optimization_guide::switches::ShouldOverrideFetchHintsTimer() ||
        features::ShouldDeferStartupActiveTabsHintsFetch()) {
      FetchHintsForActiveTabs();
    } else if (!active_tabs_hints_fetch_timer_.IsRunning()) {
      // Batch update hints with a random delay.
      active_tabs_hints_fetch_timer_.Start(
          FROM_HERE, RandomFetchDelay(), this,
          &HintsManager::FetchHintsForActiveTabs);
    }
  }
}

void HintsManager::ListenForNextUpdateForTesting(
    base::OnceClosure next_update_closure) {
  DCHECK(!next_update_closure_)
      << "Only one update closure is supported at a time";
  next_update_closure_ = std::move(next_update_closure);
}

void HintsManager::SetHintsFetcherFactoryForTesting(
    std::unique_ptr<optimization_guide::HintsFetcherFactory>
        hints_fetcher_factory) {
  hints_fetcher_factory_ = std::move(hints_fetcher_factory);
}

void HintsManager::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void HintsManager::ScheduleActiveTabsHintsFetch() {
  DCHECK(!active_tabs_hints_fetch_timer_.IsRunning());

  const base::TimeDelta active_tabs_refresh_duration =
      optimization_guide::features::GetActiveTabsFetchRefreshDuration();
  const base::TimeDelta time_since_last_fetch =
      clock_->Now() - GetLastHintsFetchAttemptTime();
  if (time_since_last_fetch >= active_tabs_refresh_duration) {
    // Fetched hints in the store should be updated and an attempt has not
    // been made in last
    // |optimization_guide::features::GetActiveTabsFetchRefreshDuration()|.
    SetLastHintsFetchAttemptTime(clock_->Now());
    active_tabs_hints_fetch_timer_.Start(
        FROM_HERE, RandomFetchDelay(), this,
        &HintsManager::FetchHintsForActiveTabs);
  } else {
    // If the fetched hints in the store are still up-to-date, set a timer
    // for when the hints need to be updated.
    base::TimeDelta fetcher_delay =
        active_tabs_refresh_duration - time_since_last_fetch;
    active_tabs_hints_fetch_timer_.Start(
        FROM_HERE, fetcher_delay, this,
        &HintsManager::ScheduleActiveTabsHintsFetch);
  }
}

const std::vector<GURL> HintsManager::GetActiveTabURLsToRefresh() {
  if (!tab_url_provider_)
    return {};

  std::vector<GURL> active_tab_urls = tab_url_provider_->GetUrlsOfActiveTabs(
      optimization_guide::features::GetActiveTabsStalenessTolerance());

  std::set<GURL> urls_to_refresh;
  for (const auto& url : active_tab_urls) {
    if (!optimization_guide::IsValidURLForURLKeyedHint(url))
      continue;

    if (!hint_cache_->HasURLKeyedEntryForURL(url))
      urls_to_refresh.insert(url);
  }
  return std::vector<GURL>(urls_to_refresh.begin(), urls_to_refresh.end());
}

void HintsManager::FetchHintsForActiveTabs() {
  active_tabs_hints_fetch_timer_.Stop();
  active_tabs_hints_fetch_timer_.Start(
      FROM_HERE,
      optimization_guide::features::GetActiveTabsFetchRefreshDuration(), this,
      &HintsManager::ScheduleActiveTabsHintsFetch);

  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          is_off_the_record_, pref_service_)) {
    return;
  }

  if (!HasOptimizationTypeToFetchFor())
    return;

  std::vector<std::string> top_hosts;
  if (top_host_provider_)
    top_hosts = top_host_provider_->GetTopHosts();

  const std::vector<GURL> active_tab_urls_to_refresh =
      GetActiveTabURLsToRefresh();

  base::UmaHistogramCounts100(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor",
      active_tab_urls_to_refresh.size());

  if (top_hosts.empty() && active_tab_urls_to_refresh.empty())
    return;

  if (!batch_update_hints_fetcher_) {
    DCHECK(hints_fetcher_factory_);
    batch_update_hints_fetcher_ = hints_fetcher_factory_->BuildInstance();
  }

  // Add hosts of active tabs to list of hosts to fetch for. Since we are mainly
  // fetching for updated information on tabs, add those to the front of the
  // list.
  if (optimization_guide::switches::IsDebugLogsEnabled()) {
    DVLOG(0) << "OptimizationGuide: ActiveTabsFetching starting fetch for: ";
    DVLOG(0) << "OptimizationGuide: Registered Optimization Types: ";
    for (const auto& optimization_type : registered_optimization_types_) {
      DVLOG(0) << "OptimizationGuide: Optimization Type: "
               << proto::OptimizationType_Name(optimization_type);
    }
    DVLOG(0) << "OptimizationGuide: URLs and Hosts: ";
  }
  base::flat_set<std::string> top_hosts_set =
      base::flat_set<std::string>(top_hosts.begin(), top_hosts.end());
  for (const auto& url : active_tab_urls_to_refresh) {
    if (optimization_guide::switches::IsDebugLogsEnabled())
      DVLOG(0) << "OptimizationGuide: URL: " << url;
    if (!url.has_host() ||
        top_hosts_set.find(url.host()) == top_hosts_set.end()) {
      continue;
    }
    if (!hint_cache_->HasHint(url.host())) {
      top_hosts_set.insert(url.host());
      top_hosts.insert(top_hosts.begin(), url.host());
      if (optimization_guide::switches::IsDebugLogsEnabled())
        DVLOG(0) << "OptimizationGuide: Host: " << url.host();
    }
  }

  batch_update_hints_fetcher_->FetchOptimizationGuideServiceHints(
      top_hosts, active_tab_urls_to_refresh, registered_optimization_types_,
      optimization_guide::proto::CONTEXT_BATCH_UPDATE, application_locale_,
      base::BindOnce(&HintsManager::OnHintsForActiveTabsFetched,
                     weak_ptr_factory_.GetWeakPtr(), top_hosts_set,
                     base::flat_set<GURL>(active_tab_urls_to_refresh.begin(),
                                          active_tab_urls_to_refresh.end())));
}

void HintsManager::OnHintsForActiveTabsFetched(
    const base::flat_set<std::string>& hosts_fetched,
    const base::flat_set<GURL>& urls_fetched,
    absl::optional<std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
        get_hints_response) {
  if (!get_hints_response)
    return;

  hint_cache_->UpdateFetchedHints(
      std::move(*get_hints_response),
      clock_->Now() +
          optimization_guide::features::GetActiveTabsFetchRefreshDuration(),
      hosts_fetched, urls_fetched,
      base::BindOnce(&HintsManager::OnFetchedActiveTabsHintsStored,
                     weak_ptr_factory_.GetWeakPtr()));
  if (optimization_guide::switches::IsDebugLogsEnabled())
    DVLOG(0) << "OptimizationGuide: OnHintsForActiveTabsFetched complete";
}

void HintsManager::OnPageNavigationHintsFetched(
    base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
    const absl::optional<GURL>& navigation_url,
    const base::flat_set<GURL>& page_navigation_urls_requested,
    const base::flat_set<std::string>& page_navigation_hosts_requested,
    absl::optional<std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
        get_hints_response) {
  if (!get_hints_response.has_value() || !get_hints_response.value()) {
    if (navigation_url) {
      CleanUpFetcherForNavigation(*navigation_url);
      PrepareToInvokeRegisteredCallbacks(*navigation_url);
    }
    return;
  }

  hint_cache_->UpdateFetchedHints(
      std::move(*get_hints_response),
      clock_->Now() +
          optimization_guide::features::GetActiveTabsFetchRefreshDuration(),
      page_navigation_hosts_requested, page_navigation_urls_requested,
      base::BindOnce(&HintsManager::OnFetchedPageNavigationHintsStored,
                     weak_ptr_factory_.GetWeakPtr(), navigation_data_weak_ptr,
                     navigation_url, page_navigation_hosts_requested));
}

void HintsManager::OnFetchedActiveTabsHintsStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN("OptimizationGuide.FetchedHints.Stored", true);

  if (!optimization_guide::features::ShouldPersistHintsToDisk()) {
    // If we aren't persisting hints to disk, there's no point in purging
    // hints from disk or starting a new fetch since at this point we should
    // just be fetching everything on page navigation and only storing
    // in-memory.
    return;
  }

  hint_cache_->PurgeExpiredFetchedHints();
}

void HintsManager::OnFetchedPageNavigationHintsStored(
    base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
    const absl::optional<GURL>& navigation_url,
    const base::flat_set<std::string>& page_navigation_hosts_requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (navigation_data_weak_ptr) {
    navigation_data_weak_ptr->set_hints_fetch_end(base::TimeTicks::Now());
  }
  base::UmaHistogramBoolean(
      "OptimizationGuide.HintsManager."
      "PageNavigationHintsReturnedBeforeDataFlushed",
      navigation_data_weak_ptr.MaybeValid());

  if (navigation_url) {
    CleanUpFetcherForNavigation(*navigation_url);
    PrepareToInvokeRegisteredCallbacks(*navigation_url);
  }
}

bool HintsManager::IsHintBeingFetchedForNavigation(const GURL& navigation_url) {
  return page_navigation_hints_fetchers_.Get(navigation_url) !=
         page_navigation_hints_fetchers_.end();
}

void HintsManager::CleanUpFetcherForNavigation(const GURL& navigation_url) {
  auto it = page_navigation_hints_fetchers_.Peek(navigation_url);
  if (it != page_navigation_hints_fetchers_.end())
    page_navigation_hints_fetchers_.Erase(it);
}

base::Time HintsManager::GetLastHintsFetchAttemptTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(pref_service_->GetInt64(
          optimization_guide::prefs::kHintsFetcherLastFetchAttempt)));
}

void HintsManager::SetLastHintsFetchAttemptTime(base::Time last_attempt_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(
      optimization_guide::prefs::kHintsFetcherLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void HintsManager::LoadHintForURL(const GURL& url, base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url.has_host()) {
    std::move(callback).Run();
    return;
  }

  LoadHintForHost(url.host(), std::move(callback));
}

void HintsManager::LoadHintForHost(const std::string& host,
                                   base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hint_cache_->LoadHint(host, base::BindOnce(&HintsManager::OnHintLoaded,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::move(callback)));
}

void HintsManager::FetchHintsForURLs(std::vector<GURL> target_urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Collect hosts, stripping duplicates, but preserving the ordering.
  optimization_guide::InsertionOrderedSet<std::string> target_hosts;
  for (const auto& url : target_urls) {
    target_hosts.insert(url.host());
  }

  if (target_hosts.empty() && target_urls.empty())
    return;

  if (!batch_update_hints_fetcher_) {
    DCHECK(hints_fetcher_factory_);
    batch_update_hints_fetcher_ = hints_fetcher_factory_->BuildInstance();
  }

  // Use the batch update hints fetcher for fetches off the SRP since we are
  // not fetching for the current navigation, even though we are fetching using
  // the page navigation context. However, since we do want to load the hints
  // returned, we pass this through to the page navigation callback.
  batch_update_hints_fetcher_->FetchOptimizationGuideServiceHints(
      target_hosts.vector(), target_urls, registered_optimization_types_,
      optimization_guide::proto::CONTEXT_BATCH_UPDATE, application_locale_,
      base::BindOnce(&HintsManager::OnPageNavigationHintsFetched,
                     weak_ptr_factory_.GetWeakPtr(), nullptr, absl::nullopt,
                     target_urls, target_hosts.set()));
}

void HintsManager::OnHintLoaded(
    base::OnceClosure callback,
    const optimization_guide::proto::Hint* loaded_hint) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record the result of loading a hint. This is used as a signal for testing.
  LOCAL_HISTOGRAM_BOOLEAN(optimization_guide::kLoadedHintLocalHistogramString,
                          loaded_hint);

  // Run the callback now that the hint is loaded. This is used as a signal by
  // tests.
  std::move(callback).Run();
}

void HintsManager::RegisterOptimizationTypes(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool should_load_new_optimization_filter = false;

  DictionaryPrefUpdate previously_registered_opt_types(
      pref_service_,
      optimization_guide::prefs::kPreviouslyRegisteredOptimizationTypes);
  for (const auto optimization_type : optimization_types) {
    if (optimization_type == optimization_guide::proto::TYPE_UNSPECIFIED)
      continue;

    if (registered_optimization_types_.find(optimization_type) !=
        registered_optimization_types_.end()) {
      continue;
    }
    registered_optimization_types_.insert(optimization_type);

    if (optimization_guide::switches::IsDebugLogsEnabled()) {
      DVLOG(0) << "OptimizationGuide: Registered new OptimizationType: "
               << optimization_guide::proto::OptimizationType_Name(
                      optimization_type);
    }

    absl::optional<double> value = previously_registered_opt_types->FindBoolKey(
        optimization_guide::proto::OptimizationType_Name(optimization_type));
    if (!value) {
      if (!is_off_the_record_ &&
          !ShouldIgnoreNewlyRegisteredOptimizationType(optimization_type) &&
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kHintsProtoOverride)) {
        should_clear_hints_for_new_type_ = true;
      }
      previously_registered_opt_types->SetBoolKey(
          optimization_guide::proto::OptimizationType_Name(optimization_type),
          true);
    }

    if (!should_load_new_optimization_filter) {
      if (optimization_types_with_filter_.find(optimization_type) !=
          optimization_types_with_filter_.end()) {
        should_load_new_optimization_filter = true;
      }
    }
  }

  // If the store is available, clear all hint state so newly registered types
  // can have their hints immediately included in hint fetches.
  if (hint_cache_->IsHintStoreAvailable() && should_clear_hints_for_new_type_) {
    ClearHostKeyedHints();
    should_clear_hints_for_new_type_ = false;
  }

  if (should_load_new_optimization_filter) {
    if (optimization_guide::switches::IsHintComponentProcessingDisabled()) {
      std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
          optimization_guide::switches::ParseComponentConfigFromCommandLine();
      if (manual_config->optimization_allowlists_size() > 0 ||
          manual_config->optimization_blocklists_size() > 0) {
        ProcessOptimizationFilters(manual_config->optimization_allowlists(),
                                   manual_config->optimization_blocklists());
      }
    } else {
      DCHECK(hints_component_info_);
      OnHintsComponentAvailable(*hints_component_info_);
    }
  } else {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
  }
}

bool HintsManager::HasLoadedOptimizationAllowlist(
    optimization_guide::proto::OptimizationType optimization_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return allowlist_optimization_filters_.find(optimization_type) !=
         allowlist_optimization_filters_.end();
}

bool HintsManager::HasLoadedOptimizationBlocklist(
    optimization_guide::proto::OptimizationType optimization_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return blocklist_optimization_filters_.find(optimization_type) !=
         blocklist_optimization_filters_.end();
}

void HintsManager::CanApplyOptimizationAsync(
    const GURL& navigation_url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  optimization_guide::OptimizationMetadata metadata;
  optimization_guide::OptimizationTypeDecision type_decision =
      CanApplyOptimization(navigation_url, optimization_type, &metadata);
  optimization_guide::OptimizationGuideDecision decision =
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(type_decision);
  // It's possible that a hint that applies to |navigation_url| will come in
  // later, so only run the callback if we are sure we can apply the decision.
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue ||
      HasAllInformationForDecisionAvailable(navigation_url,
                                            optimization_type)) {
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ApplyDecisionAsync." +
            optimization_guide::GetStringNameForOptimizationType(
                optimization_type),
        type_decision);
    std::move(callback).Run(decision, metadata);
    return;
  }

  registered_callbacks_[navigation_url][optimization_type].push_back(
      std::move(callback));
}

optimization_guide::OptimizationTypeDecision HintsManager::CanApplyOptimization(
    const GURL& navigation_url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedCanApplyOptimizationLogger scoped_logger(optimization_type,
                                                 navigation_url);
  // Clear out optimization metadata if provided.
  if (optimization_metadata)
    *optimization_metadata = {};

  // Ensure that developers register their opt types before asking the
  // optimization guide for data for that type.
  DCHECK(registered_optimization_types_.find(optimization_type) !=
         registered_optimization_types_.end());
  // If the type is not registered, we probably don't have a hint for it, so
  // just return.
  if (registered_optimization_types_.find(optimization_type) ==
      registered_optimization_types_.end()) {
    scoped_logger.set_type_decision(
        optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
    return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
  }

  // If the URL doesn't have a host, we cannot query the hint for it, so just
  // return early.
  if (!navigation_url.has_host()) {
    scoped_logger.set_type_decision(
        optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
    return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
  }
  const auto& host = navigation_url.host();

  // Check if the URL should be filtered out if we have an optimization filter
  // for the type.

  // Check if we have an allowlist loaded into memory for it, and if we do,
  // see if the URL matches anything in the filter.
  if (allowlist_optimization_filters_.find(optimization_type) !=
      allowlist_optimization_filters_.end()) {
    const auto type_decision =
        allowlist_optimization_filters_[optimization_type]->Matches(
            navigation_url)
            ? optimization_guide::OptimizationTypeDecision::
                  kAllowedByOptimizationFilter
            : optimization_guide::OptimizationTypeDecision::
                  kNotAllowedByOptimizationFilter;
    scoped_logger.set_type_decision(type_decision);
    return type_decision;
  }

  // Check if we have a blocklist loaded into memory for it, and if we do, see
  // if the URL matches anything in the filter.
  if (blocklist_optimization_filters_.find(optimization_type) !=
      blocklist_optimization_filters_.end()) {
    const auto type_decision =
        blocklist_optimization_filters_[optimization_type]->Matches(
            navigation_url)
            ? optimization_guide::OptimizationTypeDecision::
                  kNotAllowedByOptimizationFilter
            : optimization_guide::OptimizationTypeDecision::
                  kAllowedByOptimizationFilter;
    scoped_logger.set_type_decision(type_decision);
    return type_decision;
  }

  // Check if we had an optimization filter for it, but it was not loaded into
  // memory.
  if (optimization_types_with_filter_.find(optimization_type) !=
      optimization_types_with_filter_.end()) {
    scoped_logger.set_type_decision(
        optimization_guide::OptimizationTypeDecision::
            kHadOptimizationFilterButNotLoadedInTime);
    return optimization_guide::OptimizationTypeDecision::
        kHadOptimizationFilterButNotLoadedInTime;
  }

  // First, check if the optimization type is allowlisted by a URL-keyed hint.
  const optimization_guide::proto::Hint* url_keyed_hint =
      hint_cache_->GetURLKeyedHint(navigation_url);
  if (url_keyed_hint) {
    DCHECK_EQ(url_keyed_hint->page_hints_size(), 1);
    if (url_keyed_hint->page_hints_size() > 0) {
      if (IsOptimizationTypeAllowed(
              url_keyed_hint->page_hints(0).allowlisted_optimizations(),
              optimization_type, optimization_metadata)) {
        scoped_logger.set_type_decision(
            optimization_guide::OptimizationTypeDecision::kAllowedByHint);
        if (optimization_metadata && !optimization_metadata->empty())
          scoped_logger.set_has_metadata();
        return optimization_guide::OptimizationTypeDecision::kAllowedByHint;
      }
    }
  }

  // Check if we have a hint already loaded for this navigation.
  const optimization_guide::proto::Hint* loaded_hint =
      hint_cache_->GetHostKeyedHintIfLoaded(host);
  if (!loaded_hint) {
    if (hint_cache_->HasHint(host)) {
      // If we do not have a hint already loaded and we do not have one in the
      // cache, we do not know what to do with the URL so just return.
      // Otherwise, we do have information, but we just do not know it yet.
      if (optimization_guide::features::ShouldPersistHintsToDisk()) {
        scoped_logger.set_type_decision(
            optimization_guide::OptimizationTypeDecision::
                kHadHintButNotLoadedInTime);
        return optimization_guide::OptimizationTypeDecision::
            kHadHintButNotLoadedInTime;
      } else {
        scoped_logger.set_type_decision(
            optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
        return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
      }
    }

    if (IsHintBeingFetchedForNavigation(navigation_url)) {
      scoped_logger.set_type_decision(
          optimization_guide::OptimizationTypeDecision::
              kHintFetchStartedButNotAvailableInTime);
      return optimization_guide::OptimizationTypeDecision::
          kHintFetchStartedButNotAvailableInTime;
    }
    scoped_logger.set_type_decision(
        optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
    return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
  }

  if (IsOptimizationTypeAllowed(loaded_hint->allowlisted_optimizations(),
                                optimization_type, optimization_metadata)) {
    scoped_logger.set_type_decision(
        optimization_guide::OptimizationTypeDecision::kAllowedByHint);
    if (optimization_metadata && !optimization_metadata->empty())
      scoped_logger.set_has_metadata();
    return optimization_guide::OptimizationTypeDecision::kAllowedByHint;
  }

  const optimization_guide::proto::PageHint* matched_page_hint =
      loaded_hint
          ? optimization_guide::FindPageHintForURL(navigation_url, loaded_hint)
          : nullptr;
  if (!matched_page_hint) {
    scoped_logger.set_type_decision(
        optimization_guide::OptimizationTypeDecision::kNotAllowedByHint);
    return optimization_guide::OptimizationTypeDecision::kNotAllowedByHint;
  }

  bool is_allowed =
      IsOptimizationTypeAllowed(matched_page_hint->allowlisted_optimizations(),
                                optimization_type, optimization_metadata);
  const auto type_decision =
      is_allowed
          ? optimization_guide::OptimizationTypeDecision::kAllowedByHint
          : optimization_guide::OptimizationTypeDecision::kNotAllowedByHint;
  scoped_logger.set_type_decision(type_decision);
  if (optimization_metadata && !optimization_metadata->empty())
    scoped_logger.set_has_metadata();
  return type_decision;
}

void HintsManager::PrepareToInvokeRegisteredCallbacks(
    const GURL& navigation_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (registered_callbacks_.find(navigation_url) == registered_callbacks_.end())
    return;

  LoadHintForHost(
      navigation_url.host(),
      base::BindOnce(&HintsManager::OnReadyToInvokeRegisteredCallbacks,
                     weak_ptr_factory_.GetWeakPtr(), navigation_url));
}

void HintsManager::OnReadyToInvokeRegisteredCallbacks(
    const GURL& navigation_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (registered_callbacks_.find(navigation_url) ==
      registered_callbacks_.end()) {
    return;
  }

  for (auto& opt_type_and_callbacks :
       registered_callbacks_.at(navigation_url)) {
    optimization_guide::proto::OptimizationType opt_type =
        opt_type_and_callbacks.first;

    for (auto& callback : opt_type_and_callbacks.second) {
      optimization_guide::OptimizationMetadata metadata;
      optimization_guide::OptimizationTypeDecision type_decision =
          CanApplyOptimization(navigation_url, opt_type, &metadata);
      optimization_guide::OptimizationGuideDecision decision =
          GetOptimizationGuideDecisionFromOptimizationTypeDecision(
              type_decision);
      base::UmaHistogramEnumeration(
          "OptimizationGuide.ApplyDecisionAsync." +
              optimization_guide::GetStringNameForOptimizationType(opt_type),
          type_decision);
      std::move(callback).Run(decision, metadata);
    }
  }
  registered_callbacks_.erase(navigation_url);
}

bool HintsManager::HasOptimizationTypeToFetchFor() {
  if (registered_optimization_types_.empty())
    return false;

  for (const auto& optimization_type : registered_optimization_types_) {
    if (optimization_types_with_filter_.find(optimization_type) ==
        optimization_types_with_filter_.end()) {
      return true;
    }
  }
  return false;
}

bool HintsManager::IsAllowedToFetchNavigationHints(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!HasOptimizationTypeToFetchFor())
    return false;

  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          is_off_the_record_, pref_service_)) {
    return false;
  }
  DCHECK(!is_off_the_record_);

  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

void HintsManager::OnNavigationStartOrRedirect(
    OptimizationGuideNavigationData* navigation_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LoadHintForURL(navigation_data->navigation_url(), std::move(callback));

  if (optimization_guide::switches::
          DisableFetchingHintsAtNavigationStartForTesting()) {
    return;
  }

  MaybeFetchHintsForNavigation(navigation_data);
}

void HintsManager::MaybeFetchHintsForNavigation(
    OptimizationGuideNavigationData* navigation_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (registered_optimization_types_.empty())
    return;

  const GURL url = navigation_data->navigation_url();
  if (!IsAllowedToFetchNavigationHints(url))
    return;

  ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder
      race_navigation_recorder(navigation_data);

  // We expect that if the URL is being fetched for, we have already run through
  // the logic to decide if we also require fetching hints for the host.
  if (IsHintBeingFetchedForNavigation(url)) {
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchAlreadyInProgress);

    // Just set the hints fetch start to the start of the navigation, so we can
    // track whether the hint came back before commit or not.
    navigation_data->set_hints_fetch_start(navigation_data->navigation_start());
    return;
  }

  std::vector<std::string> hosts;
  std::vector<GURL> urls;
  if (!hint_cache_->HasHint(url.host())) {
    hosts.push_back(url.host());
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost);
  }

  if (!hint_cache_->HasURLKeyedEntryForURL(url)) {
    urls.push_back(url);
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchURL);
  }

  if (hosts.empty() && urls.empty()) {
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchNotAttempted);
    return;
  }

  DCHECK(hints_fetcher_factory_);
  auto it = page_navigation_hints_fetchers_.Put(
      url, hints_fetcher_factory_->BuildInstance());

  UMA_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches",
      page_navigation_hints_fetchers_.size());

  bool fetch_attempted = it->second->FetchOptimizationGuideServiceHints(
      hosts, urls, registered_optimization_types_,
      optimization_guide::proto::CONTEXT_PAGE_NAVIGATION, application_locale_,
      base::BindOnce(&HintsManager::OnPageNavigationHintsFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     navigation_data->GetWeakPtr(), url,
                     base::flat_set<GURL>(urls.begin(), urls.end()),
                     base::flat_set<std::string>(hosts.begin(), hosts.end())));
  if (fetch_attempted) {
    navigation_data->set_hints_fetch_start(base::TimeTicks::Now());

    if (!hosts.empty() && !urls.empty()) {
      race_navigation_recorder.set_race_attempt_status(
          optimization_guide::RaceNavigationFetchAttemptStatus::
              kRaceNavigationFetchHostAndURL);
      DVLOG(0) << "OptimizationGuide: Fetch hints for Navigation: ";
      DVLOG(0) << "OptimizationGuide: Registered Optimization Types: ";
      for (const auto& optimization_type : registered_optimization_types_) {
        DVLOG(0) << "OptimizationGuide: Optimization Type: "
                 << proto::OptimizationType_Name(optimization_type);
      }
      if (!hosts.empty()) {
        DVLOG(0) << "OptimizationGuide: Fetching for hosts: ";
        for (const auto& host : hosts) {
          DVLOG(0) << "OptimizationGuide: Host: " << host;
        }
      }
      if (!urls.empty()) {
        DVLOG(0) << "OptimizationGuide: Fetching for URLs: ";
        for (const auto& optimization_guide_url : urls) {
          DVLOG(0) << "OptimizationGuide: URL: " << optimization_guide_url;
        }
      }
    }
  } else {
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchNotAttempted);
  }
}

void HintsManager::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The callbacks will be invoked when the fetch request comes back, so it
  // will be cleaned up later.
  for (const auto& url : navigation_redirect_chain) {
    if (IsHintBeingFetchedForNavigation(url))
      continue;

    PrepareToInvokeRegisteredCallbacks(url);
  }
}

void HintsManager::OnDeferredStartup() {
  if (features::ShouldDeferStartupActiveTabsHintsFetch())
    InitiateHintsFetchScheduling();
}

base::WeakPtr<OptimizationGuideStore> HintsManager::hint_store() {
  return hint_cache_->hint_store();
}

optimization_guide::HintCache* HintsManager::hint_cache() {
  return hint_cache_.get();
}

optimization_guide::PushNotificationManager*
HintsManager::push_notification_manager() {
  return push_notification_manager_.get();
}

optimization_guide::HintsFetcherFactory*
HintsManager::GetHintsFetcherFactory() {
  return hints_fetcher_factory_.get();
}

bool HintsManager::HasAllInformationForDecisionAvailable(
    const GURL& navigation_url,
    optimization_guide::proto::OptimizationType optimization_type) {
  if (HasLoadedOptimizationAllowlist(optimization_type) ||
      HasLoadedOptimizationBlocklist(optimization_type)) {
    // If we have an optimization filter for the optimization type, it is
    // consulted instead of any hints that may be available.
    return true;
  }

  bool has_host_keyed_hint = hint_cache_->HasHint(navigation_url.host());
  const auto* host_keyed_hint =
      hint_cache_->GetHostKeyedHintIfLoaded(navigation_url.host());
  if (has_host_keyed_hint && host_keyed_hint == nullptr) {
    // If we have a host-keyed hint in the cache and it is not loaded, we do not
    // have all information available, regardless of whether we can fetch hints
    // or not.
    return false;
  }

  if (!IsAllowedToFetchNavigationHints(navigation_url)) {
    // If we are not allowed to fetch hints for the navigation, we have all
    // information available if the host-keyed hint we have has been loaded
    // already or we don't have a hint available.
    return host_keyed_hint != nullptr || !has_host_keyed_hint;
  }

  if (IsHintBeingFetchedForNavigation(navigation_url)) {
    // If a hint is being fetched for the navigation, then we do not have all
    // information available yet.
    return false;
  }

  // If we are allowed to fetch hints for the navigation, we only have all
  // information available for certain if we have attempted to get the URL-keyed
  // hint and if the host-keyed hint is loaded.
  return hint_cache_->HasURLKeyedEntryForURL(navigation_url) &&
         host_keyed_hint != nullptr;
}

void HintsManager::ClearFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hint_cache_->ClearFetchedHints();
  optimization_guide::HintsFetcher::ClearHostsSuccessfullyFetched(
      pref_service_);
}

void HintsManager::ClearHostKeyedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hint_cache_->ClearHostKeyedHints();
  optimization_guide::HintsFetcher::ClearHostsSuccessfullyFetched(
      pref_service_);
}

void HintsManager::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const absl::optional<optimization_guide::OptimizationMetadata>& metadata) {
  std::unique_ptr<optimization_guide::proto::Hint> hint =
      std::make_unique<optimization_guide::proto::Hint>();
  hint->set_key(url.spec());
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(optimization_type);
  if (!metadata) {
    hint_cache_->AddHintForTesting(url, std::move(hint));  // IN-TEST
    PrepareToInvokeRegisteredCallbacks(url);
    return;
  }
  if (metadata->loading_predictor_metadata()) {
    *optimization->mutable_loading_predictor_metadata() =
        *metadata->loading_predictor_metadata();
  } else if (metadata->performance_hints_metadata()) {
    *optimization->mutable_performance_hints_metadata() =
        *metadata->performance_hints_metadata();
  } else if (metadata->public_image_metadata()) {
    *optimization->mutable_public_image_metadata() =
        *metadata->public_image_metadata();
  } else if (metadata->any_metadata()) {
    *optimization->mutable_any_metadata() = *metadata->any_metadata();
  } else {
    NOTREACHED();
  }
  hint_cache_->AddHintForTesting(url, std::move(hint));  // IN-TEST
  PrepareToInvokeRegisteredCallbacks(url);
}

void HintsManager::RemoveFetchedEntriesByHintKeys(
    base::OnceClosure on_success,
    optimization_guide::proto::KeyRepresentation key_representation,
    const base::flat_set<std::string>& hint_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure the key representation is something that we expect.
  switch (key_representation) {
    case optimization_guide::proto::KeyRepresentation::HOST:
    case optimization_guide::proto::KeyRepresentation::FULL_URL:
      break;
    default:
      NOTREACHED();
      return;
  }

  if (key_representation == optimization_guide::proto::FULL_URL) {
    base::flat_set<GURL> urls_to_remove;
    base::flat_set<std::string> hosts_to_remove;
    // It is possible that the hints may have upgraded from being HOST keyed to
    // URL keyed on the server at any time. To protect against this, also remove
    // the host of the GURL from storage.
    // However, note that the opposite is not likely to happen since URL-keyed
    // hints are not persisted to disk.
    for (const std::string& url : hint_keys) {
      GURL gurl(url);
      if (!gurl.is_valid()) {
        continue;
      }
      hosts_to_remove.insert(gurl.host());
      urls_to_remove.insert(gurl);
    }

    // Also clear the HintFetcher's host pref.
    for (const std::string& host : hosts_to_remove) {
      optimization_guide::HintsFetcher::ClearSingleFetchedHost(pref_service_,
                                                               host);
    }

    hint_cache_->RemoveHintsForURLs(urls_to_remove);
    hint_cache_->RemoveHintsForHosts(std::move(on_success), hosts_to_remove);
    return;
  }

  // Also clear the HintFetcher's host pref.
  for (const std::string& host : hint_keys) {
    optimization_guide::HintsFetcher::ClearSingleFetchedHost(pref_service_,
                                                             host);
  }

  hint_cache_->RemoveHintsForHosts(std::move(on_success), hint_keys);
}

void HintsManager::PurgeFetchedEntries(base::OnceClosure on_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearFetchedHints();
  std::move(on_success).Run();
}

}  // namespace optimization_guide
