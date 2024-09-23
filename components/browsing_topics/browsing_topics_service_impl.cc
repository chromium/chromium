// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_service_impl.h"

#include <random>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/browsing_topics/browsing_topics_calculator.h"
#include "components/browsing_topics/browsing_topics_page_load_data_tracker.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/browsing_topics/util.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace browsing_topics {

namespace {

enum class NumberOfTopics {
  kZero = 0,
  kOne = 1,
  kTwo = 2,
  kThree = 3,

  kMaxValue = kThree,
};

// Returns whether the topics should all be cleared given
// `browsing_topics_data_accessible_since` and `is_topic_allowed_by_settings`.
// Returns true if `browsing_topics_data_accessible_since` is greater than the
// last calculation time.
bool ShouldClearTopicsOnStartup(
    const BrowsingTopicsState& browsing_topics_state,
    base::Time browsing_topics_data_accessible_since) {
  if (browsing_topics_state.epochs().empty()) {
    return false;
  }

  // Here we rely on the fact that `browsing_topics_data_accessible_since` can
  // only be updated to base::Time::Now() due to data deletion. So we'll either
  // need to clear all topics data, or no-op. If this assumption no longer
  // holds, we'd need to iterate over all epochs, check their calculation time,
  // and selectively delete the epochs.
  if (browsing_topics_data_accessible_since >
      browsing_topics_state.epochs().back().calculation_time()) {
    return true;
  }

  return false;
}

// Returns a vector of top topics which are disallowed and thus should be
// cleared. This could happen if the topic became disallowed when
// `browsing_topics_state` was still loading (and we didn't get a chance to
// clear it).
std::vector<privacy_sandbox::CanonicalTopic> TopTopicsToClearOnStartup(
    const BrowsingTopicsState& browsing_topics_state,
    base::RepeatingCallback<bool(const privacy_sandbox::CanonicalTopic&)>
        is_topic_allowed_by_settings) {
  DCHECK(!is_topic_allowed_by_settings.is_null());
  std::vector<privacy_sandbox::CanonicalTopic> top_topics_to_clear;
  for (const EpochTopics& epoch : browsing_topics_state.epochs()) {
    for (const TopicAndDomains& topic_and_domains :
         epoch.top_topics_and_observing_domains()) {
      if (!topic_and_domains.IsValid()) {
        continue;
      }
      privacy_sandbox::CanonicalTopic canonical_topic =
          privacy_sandbox::CanonicalTopic(topic_and_domains.topic(),
                                          epoch.taxonomy_version());
      if (!is_topic_allowed_by_settings.Run(canonical_topic)) {
        top_topics_to_clear.emplace_back(canonical_topic);
      }
    }
  }
  return top_topics_to_clear;
}

struct StartupCalculateDecision {
  bool clear_all_topics_data = true;
  base::TimeDelta next_calculation_delay;
  std::vector<privacy_sandbox::CanonicalTopic> topics_to_clear;
};

StartupCalculateDecision GetStartupCalculationDecision(
    const BrowsingTopicsState& browsing_topics_state,
    base::Time browsing_topics_data_accessible_since,
    base::RepeatingCallback<bool(const privacy_sandbox::CanonicalTopic&)>
        is_topic_allowed_by_settings) {
  // The topics have never been calculated. This could happen with a fresh
  // profile or the if the config has updated. In case of a config update, the
  // topics should have already been cleared when initializing the
  // `BrowsingTopicsState`.
  if (browsing_topics_state.next_scheduled_calculation_time().is_null()) {
    return StartupCalculateDecision{.clear_all_topics_data = false,
                                    .next_calculation_delay = base::TimeDelta(),
                                    .topics_to_clear = {}};
  }

  // This could happen when clear-on-exit is turned on and has caused the
  // cookies to be deleted on startup
  bool should_clear_all_topics_data = ShouldClearTopicsOnStartup(
      browsing_topics_state, browsing_topics_data_accessible_since);

  std::vector<privacy_sandbox::CanonicalTopic> topics_to_clear;
  if (!should_clear_all_topics_data) {
    topics_to_clear = TopTopicsToClearOnStartup(browsing_topics_state,
                                                is_topic_allowed_by_settings);
  }

  base::TimeDelta presumed_next_calculation_delay =
      browsing_topics_state.next_scheduled_calculation_time() -
      base::Time::Now();

  // The scheduled calculation time was reached before the startup.
  if (presumed_next_calculation_delay <= base::TimeDelta()) {
    return StartupCalculateDecision{
        .clear_all_topics_data = should_clear_all_topics_data,
        .next_calculation_delay = base::TimeDelta(),
        .topics_to_clear = topics_to_clear};
  }

  // This could happen if the machine time has changed since the last
  // calculation. Recalculate immediately to align with the expected schedule
  // rather than potentially stop computing for a very long time.
  if (presumed_next_calculation_delay >=
      2 * blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get()) {
    return StartupCalculateDecision{
        .clear_all_topics_data = should_clear_all_topics_data,
        .next_calculation_delay = base::TimeDelta(),
        .topics_to_clear = topics_to_clear};
  }

  return StartupCalculateDecision{
      .clear_all_topics_data = should_clear_all_topics_data,
      .next_calculation_delay = presumed_next_calculation_delay,
      .topics_to_clear = topics_to_clear};
}

void RecordBrowsingTopicsApiResultMetrics(ApiAccessResult result,
                                          content::RenderFrameHost* main_frame,
                                          bool is_get_topics_request) {
  // The `BrowsingTopics_DocumentBrowsingTopicsApiResult2` event is only
  // recorded for request that gets the topics.
  if (!is_get_topics_request) {
    return;
  }

  base::UmaHistogramEnumeration("BrowsingTopics.Result.Status", result);

  if (result == browsing_topics::ApiAccessResult::kSuccess) {
    return;
  }

  CHECK(!main_frame->IsInLifecycleState(
      content::RenderFrameHost::LifecycleState::kPrerendering));
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2 builder(
      main_frame->GetPageUkmSourceId());
  builder.SetFailureReason(static_cast<int64_t>(result));

  builder.Record(ukm_recorder->Get());
}

void RecordBrowsingTopicsApiResultMetrics(
    const std::vector<CandidateTopic>& valid_candidate_topics,
    content::RenderFrameHost* main_frame) {
  CHECK(!main_frame->IsInLifecycleState(
      content::RenderFrameHost::LifecycleState::kPrerendering));
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2 builder(
      main_frame->GetPageUkmSourceId());

  int real_count = 0;
  int fake_count = 0;
  int filtered_count = 0;

  for (size_t i = 0; i < 3u && valid_candidate_topics.size() > i; ++i) {
    const CandidateTopic& candidate_topic = valid_candidate_topics[i];

    DCHECK(candidate_topic.IsValid());

    if (candidate_topic.should_be_filtered()) {
      filtered_count += 1;
    } else {
      candidate_topic.is_true_topic() ? real_count += 1 : fake_count += 1;
    }

    if (i == 0) {
      builder.SetCandidateTopic0(candidate_topic.topic().value())
          .SetCandidateTopic0IsTrueTopTopic(candidate_topic.is_true_topic())
          .SetCandidateTopic0ShouldBeFiltered(
              candidate_topic.should_be_filtered())
          .SetCandidateTopic0TaxonomyVersion(candidate_topic.taxonomy_version())
          .SetCandidateTopic0ModelVersion(candidate_topic.model_version());
    } else if (i == 1) {
      builder.SetCandidateTopic1(candidate_topic.topic().value())
          .SetCandidateTopic1IsTrueTopTopic(candidate_topic.is_true_topic())
          .SetCandidateTopic1ShouldBeFiltered(
              candidate_topic.should_be_filtered())
          .SetCandidateTopic1TaxonomyVersion(candidate_topic.taxonomy_version())
          .SetCandidateTopic1ModelVersion(candidate_topic.model_version());
    } else {
      DCHECK_EQ(i, 2u);
      builder.SetCandidateTopic2(candidate_topic.topic().value())
          .SetCandidateTopic2IsTrueTopTopic(candidate_topic.is_true_topic())
          .SetCandidateTopic2ShouldBeFiltered(
              candidate_topic.should_be_filtered())
          .SetCandidateTopic2TaxonomyVersion(candidate_topic.taxonomy_version())
          .SetCandidateTopic2ModelVersion(candidate_topic.model_version());
    }
  }

  CHECK_GE(real_count, 0);
  CHECK_GE(fake_count, 0);
  CHECK_GE(filtered_count, 0);
  CHECK_LE(real_count, static_cast<int>(NumberOfTopics::kMaxValue));
  CHECK_LE(fake_count, static_cast<int>(NumberOfTopics::kMaxValue));
  CHECK_LE(filtered_count, static_cast<int>(NumberOfTopics::kMaxValue));

  base::UmaHistogramEnumeration("BrowsingTopics.Result.RealTopicCount",
                                static_cast<NumberOfTopics>(real_count));
  base::UmaHistogramEnumeration("BrowsingTopics.Result.FakeTopicCount",
                                static_cast<NumberOfTopics>(fake_count));
  base::UmaHistogramEnumeration("BrowsingTopics.Result.FilteredTopicCount",
                                static_cast<NumberOfTopics>(filtered_count));

  builder.Record(ukm_recorder->Get());
}

// Represents the action type of the request.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BrowsingTopicsApiActionType {
  // Get topics via document.browsingTopics({skipObservation: true}).
  kGetViaDocumentApi = 0,

  // Get and observe topics via the document.browsingTopics().
  kGetAndObserveViaDocumentApi = 1,

  // Get topics via fetch(<url>, {browsingTopics: true}) or via the analogous
  // XHR request.
  kGetViaFetchLikeApi = 2,

  // Observe topics via the "Sec-Browsing-Topics: ?1" response header for the
  // fetch(<url>, {browsingTopics: true}) request, or for the analogous XHR
  // request.
  kObserveViaFetchLikeApi = 3,

  // Get topics via <iframe src=[url] browsingtopics>.
  kGetViaIframeAttributeApi = 4,

  // Observe topics via the "Sec-Browsing-Topics: ?1" response header for the
  // <iframe src=[url] browsingtopics> request.
  kObserveViaIframeAttributeApi = 5,

  kMaxValue = kObserveViaIframeAttributeApi,
};

void RecordBrowsingTopicsApiActionTypeMetrics(ApiCallerSource caller_source,
                                              bool get_topics,
                                              bool observe) {
  static constexpr char kBrowsingTopicsApiActionTypeHistogramId[] =
      "BrowsingTopics.ApiActionType";

  if (caller_source == ApiCallerSource::kJavaScript) {
    DCHECK(get_topics);

    if (!observe) {
      base::UmaHistogramEnumeration(
          kBrowsingTopicsApiActionTypeHistogramId,
          BrowsingTopicsApiActionType::kGetViaDocumentApi);
      return;
    }

    base::UmaHistogramEnumeration(
        kBrowsingTopicsApiActionTypeHistogramId,
        BrowsingTopicsApiActionType::kGetAndObserveViaDocumentApi);

    return;
  }

  if (caller_source == ApiCallerSource::kIframeAttribute) {
    if (get_topics) {
      DCHECK(!observe);

      base::UmaHistogramEnumeration(
          kBrowsingTopicsApiActionTypeHistogramId,
          BrowsingTopicsApiActionType::kGetViaIframeAttributeApi);
      return;
    }

    DCHECK(observe);
    base::UmaHistogramEnumeration(
        kBrowsingTopicsApiActionTypeHistogramId,
        BrowsingTopicsApiActionType::kObserveViaIframeAttributeApi);

    return;
  }

  DCHECK_EQ(caller_source, ApiCallerSource::kFetch);

  if (get_topics) {
    DCHECK(!observe);

    base::UmaHistogramEnumeration(
        kBrowsingTopicsApiActionTypeHistogramId,
        BrowsingTopicsApiActionType::kGetViaFetchLikeApi);
    return;
  }

  DCHECK(observe);
  base::UmaHistogramEnumeration(
      kBrowsingTopicsApiActionTypeHistogramId,
      BrowsingTopicsApiActionType::kObserveViaFetchLikeApi);
}

std::set<HashedDomain> GetAllObservingDomains(
    const BrowsingTopicsState& browsing_topics_state) {
  std::set<HashedDomain> observing_domains;
  for (const EpochTopics& epoch : browsing_topics_state.epochs()) {
    for (const auto& topic_and_domains :
         epoch.top_topics_and_observing_domains()) {
      observing_domains.insert(topic_and_domains.hashed_domains().begin(),
                               topic_and_domains.hashed_domains().end());
    }
  }
  return observing_domains;
}

}  // namespace

BrowsingTopicsServiceImpl::~BrowsingTopicsServiceImpl() = default;

BrowsingTopicsServiceImpl::BrowsingTopicsServiceImpl(
    const base::FilePath& profile_path,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    std::unique_ptr<Annotator> annotator,
    TopicAccessedCallback topic_accessed_callback)
    : privacy_sandbox_settings_(privacy_sandbox_settings),
      history_service_(history_service),
      site_data_manager_(site_data_manager),
      browsing_topics_state_(
          profile_path,
          base::BindOnce(
              &BrowsingTopicsServiceImpl::OnBrowsingTopicsStateLoaded,
              base::Unretained(this))),
      annotator_(std::move(annotator)),
      topic_accessed_callback_(std::move(topic_accessed_callback)),
      session_start_time_(base::Time::Now()) {
  DCHECK(topic_accessed_callback_);
  privacy_sandbox_settings_observation_.Observe(privacy_sandbox_settings);
  history_service_observation_.Observe(history_service);
}

bool BrowsingTopicsServiceImpl::HandleTopicsWebApi(
    const url::Origin& context_origin,
    content::RenderFrameHost* main_frame,
    ApiCallerSource caller_source,
    bool get_topics,
    bool observe,
    std::vector<blink::mojom::EpochTopicPtr>& topics) {
  DCHECK(topics.empty());
  DCHECK(get_topics || observe);

  if (is_shutting_down_) {
    return false;
  }

  RecordBrowsingTopicsApiActionTypeMetrics(caller_source, get_topics, observe);

  if (!browsing_topics_state_loaded_) {
    RecordBrowsingTopicsApiResultMetrics(ApiAccessResult::kStateNotReady,
                                         main_frame, get_topics);
    return false;
  }

  if (!privacy_sandbox_settings_->IsTopicsAllowed()) {
    RecordBrowsingTopicsApiResultMetrics(
        ApiAccessResult::kAccessDisallowedBySettings, main_frame, get_topics);
    return false;
  }

  if (!privacy_sandbox_settings_->IsTopicsAllowedForContext(
          /*top_frame_origin=*/main_frame->GetLastCommittedOrigin(),
          context_origin.GetURL(), main_frame)) {
    RecordBrowsingTopicsApiResultMetrics(
        ApiAccessResult::kAccessDisallowedBySettings, main_frame, get_topics);
    return false;
  }

  RecordBrowsingTopicsApiResultMetrics(ApiAccessResult::kSuccess, main_frame,
                                       get_topics);

  std::string context_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          context_origin.GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  HashedDomain hashed_context_domain = HashContextDomainForStorage(
      browsing_topics_state_.hmac_key(), context_domain);

  // Track the API usage context after the permissions check.
  BrowsingTopicsPageLoadDataTracker::GetOrCreateForPage(main_frame->GetPage())
      ->OnBrowsingTopicsApiUsed(hashed_context_domain, context_domain,
                                history_service_, observe);

  if (!get_topics) {
    return true;
  }

  std::string top_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          main_frame->GetLastCommittedOrigin().GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  std::vector<CandidateTopic> valid_candidate_topics;

  for (const EpochTopics* epoch :
       browsing_topics_state_.EpochsForSite(top_domain)) {
    CandidateTopic candidate_topic = epoch->CandidateTopicForSite(
        top_domain, hashed_context_domain, browsing_topics_state_.hmac_key());

    if (!candidate_topic.IsValid()) {
      continue;
    }

    // Although a top topic can never be in the disallowed state, the returned
    // `candidate_topic` may be the random one. Thus we still need this check.
    if (!privacy_sandbox_settings_->IsTopicAllowed(
            privacy_sandbox::CanonicalTopic(
                candidate_topic.topic(), candidate_topic.taxonomy_version()))) {
      DCHECK(!candidate_topic.is_true_topic());
      continue;
    }

    valid_candidate_topics.push_back(std::move(candidate_topic));
  }

  RecordBrowsingTopicsApiResultMetrics(valid_candidate_topics, main_frame);

  for (const CandidateTopic& candidate_topic : valid_candidate_topics) {
    if (candidate_topic.should_be_filtered()) {
      continue;
    }

    // `PageSpecificContentSettings` should only observe true top topics
    // accessed on the page. It's okay to notify the same topic multiple
    // times even though duplicate topics will be removed in the end.
    if (candidate_topic.is_true_topic()) {
      privacy_sandbox::CanonicalTopic canonical_topic(
          candidate_topic.topic(), candidate_topic.taxonomy_version());
      topic_accessed_callback_.Run(main_frame, context_origin,
                                   /*blocked_by_policy=*/false,
                                   canonical_topic);
    }

    auto result_topic = blink::mojom::EpochTopic::New();
    result_topic->topic = candidate_topic.topic().value();
    result_topic->config_version = base::StrCat(
        {"chrome.", base::NumberToString(candidate_topic.config_version())});
    result_topic->model_version =
        base::NumberToString(candidate_topic.model_version());
    result_topic->taxonomy_version =
        base::NumberToString(candidate_topic.taxonomy_version());
    result_topic->version = base::StrCat({result_topic->config_version, ":",
                                          result_topic->taxonomy_version, ":",
                                          result_topic->model_version});
    topics.emplace_back(std::move(result_topic));
  }

  // Sort result based on the version first, and then based on the topic ID.
  // This groups the topics with the same version together, so that when
  // transforming into the header format, all duplicate versions can be omitted.
  std::sort(topics.begin(), topics.end(),
            [](const blink::mojom::EpochTopicPtr& left,
               const blink::mojom::EpochTopicPtr& right) {
              if (left->version != right->version) {
                return left->version < right->version;
              }

              return left->topic < right->topic;
            });

  // Remove duplicate entries.
  topics.erase(std::unique(topics.begin(), topics.end()), topics.end());

  return true;
}

int BrowsingTopicsServiceImpl::NumVersionsInEpochs(
    const url::Origin& main_frame_origin) const {
  CHECK(browsing_topics_state_loaded_);
  CHECK(!is_shutting_down_);
  CHECK(privacy_sandbox_settings_->IsTopicsAllowed());

  std::string main_frame_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          main_frame_origin.GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  std::set<std::pair<int, int64_t>> distinct_versions;
  for (const EpochTopics* epoch :
       browsing_topics_state_.EpochsForSite(main_frame_domain)) {
    if (epoch->HasValidVersions()) {
      distinct_versions.emplace(epoch->taxonomy_version(),
                                epoch->model_version());
    }
  }

  return distinct_versions.size();
}

void BrowsingTopicsServiceImpl::GetBrowsingTopicsStateForWebUi(
    bool calculate_now,
    mojom::PageHandler::GetBrowsingTopicsStateCallback callback) {
  if (!browsing_topics_state_loaded_) {
    std::move(callback).Run(
        mojom::WebUIGetBrowsingTopicsStateResult::NewOverrideStatusMessage(
            "State loading hasn't finished. Please retry shortly."));
    return;
  }

  if (is_shutting_down_) {
    std::move(callback).Run(
        mojom::WebUIGetBrowsingTopicsStateResult::NewOverrideStatusMessage(
            "BrowsingTopicsService is shutting down."));
    return;
  }

  // If a calculation is already in progress, get the webui topics state after
  // the calculation is done. Do this regardless of whether `calculate_now` is
  // true, i.e. if `calculate_now` is true, this request is effectively merged
  // with the in progress calculation.
  if (topics_calculator_) {
    get_state_for_webui_callbacks_.push_back(std::move(callback));
    return;
  }

  DCHECK(schedule_calculate_timer_.IsRunning());

  if (calculate_now) {
    get_state_for_webui_callbacks_.push_back(std::move(callback));
    schedule_calculate_timer_.Stop();
    CalculateBrowsingTopics(/*is_manually_triggered=*/true,
                            /*previous_timeout_count=*/0);
    return;
  }

  site_data_manager_->GetContextDomainsFromHashedContextDomains(
      GetAllObservingDomains(browsing_topics_state_),
      base::BindOnce(
          &BrowsingTopicsServiceImpl::GetBrowsingTopicsStateForWebUiHelper,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<privacy_sandbox::CanonicalTopic>
BrowsingTopicsServiceImpl::GetTopTopicsForDisplay() const {
  if (!browsing_topics_state_loaded_ || is_shutting_down_) {
    return {};
  }

  std::vector<privacy_sandbox::CanonicalTopic> result;

  for (const EpochTopics& epoch : browsing_topics_state_.epochs()) {
    DCHECK_LE(epoch.padded_top_topics_start_index(),
              epoch.top_topics_and_observing_domains().size());

    for (size_t i = 0; i < epoch.padded_top_topics_start_index(); ++i) {
      const TopicAndDomains& topic_and_domains =
          epoch.top_topics_and_observing_domains()[i];

      if (!topic_and_domains.IsValid()) {
        continue;
      }

      // A top topic can never be in the disallowed state (i.e. it will be
      // cleared when it becomes diallowed).
      DCHECK(privacy_sandbox_settings_->IsTopicAllowed(
          privacy_sandbox::CanonicalTopic(topic_and_domains.topic(),
                                          epoch.taxonomy_version())));

      result.emplace_back(topic_and_domains.topic(), epoch.taxonomy_version());
    }
  }

  return result;
}

void BrowsingTopicsServiceImpl::ValidateCalculationSchedule() {
  if (!browsing_topics_state_loaded_ || topics_calculator_ ||
      is_shutting_down_ || recorded_calculation_did_not_occur_metrics_) {
    return;
  }

  // Verify the alignment of the calculation schedule with the topics state's
  // scheduled time, allowing for a one-minute flex window to accommodate the
  // timer's imprecision. In the event of a discrepancy, log metrics to aid in
  // troubleshooting.
  base::TimeDelta elapsed_since_scheduled_time =
      base::Time::Now() -
      browsing_topics_state_.next_scheduled_calculation_time();

  if (elapsed_since_scheduled_time > base::Minutes(1)) {
    base::UmaHistogramExactLinear(
        "BrowsingTopics.EpochTopicsCalculation.DidNotOccurAtScheduledTime."
        "DaysSinceSessionStart",
        (base::Time::Now() - session_start_time_).InDays(),
        /*exclusive_max=*/30);
    base::UmaHistogramExactLinear(
        "BrowsingTopics.EpochTopicsCalculation.DidNotOccurAtScheduledTime."
        "HoursSinceScheduledTime",
        elapsed_since_scheduled_time.InHours(),
        /*exclusive_max=*/30);
    base::UmaHistogramBoolean(
        "BrowsingTopics.EpochTopicsCalculation.DidNotOccurAtScheduledTime."
        "CalculationTimerIsRunning",
        schedule_calculate_timer_.IsRunning());

    base::TimeDelta remaining_time_in_calculator_timer =
        schedule_calculate_timer_.desired_run_time() - base::Time::Now();

    base::UmaHistogramBoolean(
        "BrowsingTopics.EpochTopicsCalculation.DidNotOccurAtScheduledTime."
        "RemainingTimeInCalculationTimerIsPositive",
        remaining_time_in_calculator_timer.is_positive());

    if (remaining_time_in_calculator_timer.is_positive()) {
      base::UmaHistogramExactLinear(
          "BrowsingTopics.EpochTopicsCalculation.DidNotOccurAtScheduledTime."
          "RemainingDaysInCalculationTimer",
          remaining_time_in_calculator_timer.InDays(),
          /*exclusive_max=*/30);
    }

    recorded_calculation_did_not_occur_metrics_ = true;
  }
}

Annotator* BrowsingTopicsServiceImpl::GetAnnotator() {
  return annotator_.get();
}

void BrowsingTopicsServiceImpl::ClearTopic(
    const privacy_sandbox::CanonicalTopic& canonical_topic) {
  if (!browsing_topics_state_loaded_ || is_shutting_down_) {
    return;
  }

  browsing_topics_state_.ClearTopic(canonical_topic.topic_id());
}

void BrowsingTopicsServiceImpl::ClearTopicsDataForOrigin(
    const url::Origin& origin) {
  if (!browsing_topics_state_loaded_ || is_shutting_down_) {
    return;
  }

  std::string context_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin.GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  HashedDomain hashed_context_domain = HashContextDomainForStorage(
      browsing_topics_state_.hmac_key(), context_domain);

  browsing_topics_state_.ClearContextDomain(hashed_context_domain);
  site_data_manager_->ClearContextDomain(hashed_context_domain);
}

void BrowsingTopicsServiceImpl::ClearAllTopicsData() {
  if (!browsing_topics_state_loaded_ || is_shutting_down_) {
    return;
  }

  browsing_topics_state_.ClearAllTopics();
  site_data_manager_->ExpireDataBefore(base::Time::Now());
}

std::unique_ptr<BrowsingTopicsCalculator>
BrowsingTopicsServiceImpl::CreateCalculator(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    Annotator* annotator,
    const base::circular_deque<EpochTopics>& epochs,
    bool is_manually_triggered,
    int previous_timeout_count,
    base::Time session_start_time,
    BrowsingTopicsCalculator::CalculateCompletedCallback callback) {
  CHECK(!is_shutting_down_);
  return std::make_unique<BrowsingTopicsCalculator>(
      privacy_sandbox_settings, history_service, site_data_manager, annotator,
      epochs, is_manually_triggered, previous_timeout_count, session_start_time,
      std::move(callback));
}

const BrowsingTopicsState& BrowsingTopicsServiceImpl::browsing_topics_state() {
  return browsing_topics_state_;
}

void BrowsingTopicsServiceImpl::ScheduleBrowsingTopicsCalculation(
    bool is_manually_triggered,
    int previous_timeout_count,
    base::TimeDelta delay,
    bool persist_calculation_time) {
  DCHECK(browsing_topics_state_loaded_);

  if (persist_calculation_time) {
    browsing_topics_state_.UpdateNextScheduledCalculationTime(delay);
  }

  // `this` owns the timer, which is automatically cancelled on destruction, so
  // base::Unretained(this) is safe.
  schedule_calculate_timer_.Start(
      FROM_HERE, base::Time::Now() + delay,
      base::BindOnce(&BrowsingTopicsServiceImpl::CalculateBrowsingTopics,
                     base::Unretained(this), is_manually_triggered,
                     previous_timeout_count));
}

void BrowsingTopicsServiceImpl::CalculateBrowsingTopics(
    bool is_manually_triggered,
    int previous_timeout_count) {
  DCHECK(browsing_topics_state_loaded_);

  DCHECK(!topics_calculator_);

  if (is_shutting_down_) {
    return;
  }

  // `this` owns `topics_calculator_` so `topics_calculator_` should not invoke
  // the callback once it's destroyed.
  topics_calculator_ = CreateCalculator(
      privacy_sandbox_settings_, history_service_, site_data_manager_,
      annotator_.get(), browsing_topics_state_.epochs(), is_manually_triggered,
      previous_timeout_count, session_start_time_,
      base::BindOnce(
          &BrowsingTopicsServiceImpl::OnCalculateBrowsingTopicsCompleted,
          base::Unretained(this)));
}

void BrowsingTopicsServiceImpl::OnCalculateBrowsingTopicsCompleted(
    EpochTopics epoch_topics) {
  CHECK(browsing_topics_state_loaded_);
  CHECK(topics_calculator_);
  CHECK(!schedule_calculate_timer_.IsRunning());
  CHECK(!is_shutting_down_);

  const std::optional<CalculatorResultStatus>& status =
      epoch_topics.calculator_result_status();
  CHECK(status);
  CHECK_NE(*status, CalculatorResultStatus::kTerminated);

  bool is_manually_triggered = topics_calculator_->is_manually_triggered();
  int previous_timeout_count = topics_calculator_->previous_timeout_count();
  topics_calculator_.reset();

  // If a calculation fails due to hanging, retry it.
  if (DoesCalculationFailDueToHanging(*status)) {
    CHECK_LE(blink::features::kBrowsingTopicsFirstTimeoutRetryDelay.Get(),
             blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get());

    // Retry with exponential backoff for up to 5 times. The delay shouldn't be
    // greater than an epoch. After 5 retries with exponential backoff, resume
    // to the epoch cadence.
    base::TimeDelta delay =
        blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get();

    if (previous_timeout_count < 5) {
      base::TimeDelta exponential_backoff_delay =
          blink::features::kBrowsingTopicsFirstTimeoutRetryDelay.Get() *
          (1LL << previous_timeout_count);

      delay = std::min(delay, exponential_backoff_delay);
    }

    ScheduleBrowsingTopicsCalculation(is_manually_triggered,
                                      previous_timeout_count + 1, delay,
                                      /*persist_calculation_time=*/true);
    return;
  }

  if (!browsing_topics_state_.epochs().empty()) {
    // Use 24 days as the max value, because 24 days is the maximum number of
    // days that works with UmaHistogramCustomTimes due to its conversion of
    // times into milliseconds. We expect most values to be around
    // `kBrowsingTopicsTimePeriodPerEpoch`.
    base::UmaHistogramCustomTimes(
        "BrowsingTopics.EpochTopicsCalculation.TimeBetweenCalculations",
        epoch_topics.calculation_time() -
            browsing_topics_state_.epochs().back().calculation_time(),
        /*min=*/base::Seconds(1), /*max=*/base::Days(24), /*buckets=*/100);
  }

  std::optional<EpochTopics> maybe_removed_epoch =
      browsing_topics_state_.AddEpoch(std::move(epoch_topics));
  if (maybe_removed_epoch.has_value()) {
    site_data_manager_->ExpireDataBefore(
        maybe_removed_epoch->calculation_time() -
        blink::features::
                kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering
                    .Get() *
            blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get());
  }

  ScheduleBrowsingTopicsCalculation(
      /*is_manually_triggered=*/false,
      /*previous_timeout_count=*/0,
      blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get(),
      /*persist_calculation_time=*/true);

  for (auto& callback : get_state_for_webui_callbacks_) {
    site_data_manager_->GetContextDomainsFromHashedContextDomains(
        GetAllObservingDomains(browsing_topics_state_),
        base::BindOnce(
            &BrowsingTopicsServiceImpl::GetBrowsingTopicsStateForWebUiHelper,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
  get_state_for_webui_callbacks_.clear();
}

void BrowsingTopicsServiceImpl::OnBrowsingTopicsStateLoaded() {
  DCHECK(!browsing_topics_state_loaded_);
  if (is_shutting_down_) {
    return;
  }
  browsing_topics_state_loaded_ = true;

  base::Time browsing_topics_data_sccessible_since =
      privacy_sandbox_settings_->TopicsDataAccessibleSince();

  StartupCalculateDecision decision = GetStartupCalculationDecision(
      browsing_topics_state_, browsing_topics_data_sccessible_since,
      base::BindRepeating(
          &privacy_sandbox::PrivacySandboxSettings::IsTopicAllowed,
          base::Unretained(privacy_sandbox_settings_)));

  if (decision.clear_all_topics_data) {
    browsing_topics_state_.ClearAllTopics();
  } else if (!decision.topics_to_clear.empty()) {
    for (const privacy_sandbox::CanonicalTopic& canonical_topic :
         decision.topics_to_clear) {
      browsing_topics_state_.ClearTopic(canonical_topic.topic_id());
    }
  }

  site_data_manager_->ExpireDataBefore(browsing_topics_data_sccessible_since);

  browsing_topics_state_.ScheduleEpochsExpiration();

  ScheduleBrowsingTopicsCalculation(
      /*is_manually_triggered=*/false,
      /*previous_timeout_count=*/0, decision.next_calculation_delay,
      /*persist_calculation_time=*/false);
}

void BrowsingTopicsServiceImpl::Shutdown() {
  is_shutting_down_ = true;
  // Reset `topics_calculator_` if it's set because it holds a raw_ptr to
  // `privacy_sandbox_settings_` and `history_service_`.
  if (topics_calculator_) {
    topics_calculator_.reset();
  }
  // Reset `annotator_` because it holds a raw_ptr to the
  // the per-profile `OptimizationGuideKeyedService`.
  annotator_.reset();
  privacy_sandbox_settings_observation_.Reset();
  history_service_observation_.Reset();
  privacy_sandbox_settings_ = nullptr;
  history_service_ = nullptr;
}

void BrowsingTopicsServiceImpl::OnTopicsDataAccessibleSinceUpdated() {
  CHECK(!is_shutting_down_);
  if (!browsing_topics_state_loaded_) {
    return;
  }

  // Here we rely on the fact that `browsing_topics_data_accessible_since` can
  // only be updated to base::Time::Now() due to data deletion. In this case, we
  // should just clear all topics.
  browsing_topics_state_.ClearAllTopics();
  site_data_manager_->ExpireDataBefore(
      privacy_sandbox_settings_->TopicsDataAccessibleSince());

  // Abort the outstanding topics calculation and restart immediately.
  if (topics_calculator_) {
    DCHECK(!schedule_calculate_timer_.IsRunning());

    bool is_manually_triggered = topics_calculator_->is_manually_triggered();
    int previous_timeout_count = topics_calculator_->previous_timeout_count();
    topics_calculator_.reset();
    CalculateBrowsingTopics(is_manually_triggered, previous_timeout_count);
  }
}

void BrowsingTopicsServiceImpl::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  CHECK(!is_shutting_down_);
  if (!browsing_topics_state_loaded_) {
    return;
  }

  // Ignore invalid time_range.
  if (!deletion_info.IsAllHistory() && !deletion_info.time_range().IsValid()) {
    return;
  }

  for (size_t i = 0; i < browsing_topics_state_.epochs().size(); ++i) {
    const EpochTopics& epoch_topics = browsing_topics_state_.epochs()[i];

    if (epoch_topics.empty()) {
      continue;
    }

    // The typical case is assumed here. We cannot always derive the original
    // history start time, as the necessary data (e.g. its previous epoch's
    // calculation time) may have been gone.
    base::Time history_data_start_time =
        epoch_topics.calculation_time() -
        blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get();

    bool time_range_overlap =
        epoch_topics.calculation_time() >= deletion_info.time_range().begin() &&
        history_data_start_time <= deletion_info.time_range().end();

    if (time_range_overlap) {
      browsing_topics_state_.ClearOneEpoch(i);
    }
  }

  // If there's an outstanding topics calculation, abort and restart it.
  if (topics_calculator_) {
    DCHECK(!schedule_calculate_timer_.IsRunning());

    bool is_manually_triggered = topics_calculator_->is_manually_triggered();
    int previous_timeout_count = topics_calculator_->previous_timeout_count();
    topics_calculator_.reset();
    CalculateBrowsingTopics(is_manually_triggered, previous_timeout_count);
  }
}

void BrowsingTopicsServiceImpl::GetBrowsingTopicsStateForWebUiHelper(
    mojom::PageHandler::GetBrowsingTopicsStateCallback callback,
    std::map<HashedDomain, std::string> hashed_to_unhashed_context_domains) {
  DCHECK(browsing_topics_state_loaded_);
  DCHECK(!topics_calculator_);

  if (is_shutting_down_) {
    std::move(callback).Run(
        mojom::WebUIGetBrowsingTopicsStateResult::NewOverrideStatusMessage(
            "BrowsingTopicsService is shutting down."));
    return;
  }

  auto webui_state = mojom::WebUIBrowsingTopicsState::New();

  webui_state->next_scheduled_calculation_time =
      browsing_topics_state_.next_scheduled_calculation_time();

  for (const EpochTopics& epoch : browsing_topics_state_.epochs()) {
    DCHECK_LE(epoch.padded_top_topics_start_index(),
              epoch.top_topics_and_observing_domains().size());

    // Note: for a failed epoch calculation, the default zero-initialized values
    // will be displayed in the Web UI.
    auto webui_epoch = mojom::WebUIEpoch::New();
    webui_epoch->calculation_time = epoch.calculation_time();
    webui_epoch->model_version = base::NumberToString(epoch.model_version());
    webui_epoch->taxonomy_version =
        base::NumberToString(epoch.taxonomy_version());

    for (size_t i = 0; i < epoch.top_topics_and_observing_domains().size();
         ++i) {
      const TopicAndDomains& topic_and_domains =
          epoch.top_topics_and_observing_domains()[i];

      privacy_sandbox::CanonicalTopic canonical_topic =
          privacy_sandbox::CanonicalTopic(topic_and_domains.topic(),
                                          epoch.taxonomy_version());

      std::vector<std::string> webui_observed_by_domains;
      webui_observed_by_domains.reserve(
          topic_and_domains.hashed_domains().size());
      for (const HashedDomain& hashed_domain :
           topic_and_domains.hashed_domains()) {
        auto it = hashed_to_unhashed_context_domains.find(hashed_domain);
        if (it != hashed_to_unhashed_context_domains.end()) {
          webui_observed_by_domains.push_back(it->second);
        } else {
          // Default to the hashed value if we don't have the original.
          webui_observed_by_domains.push_back(
              base::NumberToString(hashed_domain.value()));
        }
      }

      // Note: if the topic is invalid (i.e. cleared), the output `topic_id`
      // will be 0; if the topic is invalid, or if the taxonomy version isn't
      // recognized by this Chrome binary, the output `topic_name` will be
      // "Unknown".
      auto webui_topic = mojom::WebUITopic::New();
      webui_topic->topic_id = topic_and_domains.topic().value();
      webui_topic->topic_name = canonical_topic.GetLocalizedRepresentation();
      webui_topic->is_real_topic = (i < epoch.padded_top_topics_start_index());
      webui_topic->observed_by_domains = std::move(webui_observed_by_domains);

      webui_epoch->topics.push_back(std::move(webui_topic));
    }

    webui_state->epochs.push_back(std::move(webui_epoch));
  }

  // Reorder the epochs from latest to oldest.
  base::ranges::reverse(webui_state->epochs);

  std::move(callback).Run(
      mojom::WebUIGetBrowsingTopicsStateResult::NewBrowsingTopicsState(
          std::move(webui_state)));
}

}  // namespace browsing_topics
