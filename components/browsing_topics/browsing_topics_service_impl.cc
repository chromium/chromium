// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_service_impl.h"

#include <random>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/browsing_topics/browsing_topics_calculator.h"
#include "components/browsing_topics/browsing_topics_page_load_data_tracker.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/browsing_topics/util.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace browsing_topics {

namespace {

// Returns whether the topics should all be cleared given
// `browsing_topics_data_accessible_since` and `is_topic_allowed_by_settings`.
// Returns true if `browsing_topics_data_accessible_since` is greater than the
// last calculation time, or if any top topic is disallowed from the settings.
// The latter could happen if the topic became disallowed when
// `browsing_topics_state` was still loading (and we didn't get a chance to
// clear it). This is an unlikely edge case, so it's fine to over-delete.
bool ShouldClearTopicsOnStartup(
    const BrowsingTopicsState& browsing_topics_state,
    base::Time browsing_topics_data_accessible_since,
    base::RepeatingCallback<bool(const privacy_sandbox::CanonicalTopic&)>
        is_topic_allowed_by_settings) {
  DCHECK(!is_topic_allowed_by_settings.is_null());

  if (browsing_topics_state.epochs().empty())
    return false;

  // Here we rely on the fact that `browsing_topics_data_accessible_since` can
  // only be updated to base::Time::Now() due to data deletion. So we'll either
  // need to clear all topics data, or no-op. If this assumption no longer
  // holds, we'd need to iterate over all epochs, check their calculation time,
  // and selectively delete the epochs.
  if (browsing_topics_data_accessible_since >
      browsing_topics_state.epochs().back().calculation_time()) {
    return true;
  }

  for (const EpochTopics& epoch : browsing_topics_state.epochs()) {
    for (const TopicAndDomains& topic_and_domains :
         epoch.top_topics_and_observing_domains()) {
      if (!topic_and_domains.IsValid())
        continue;

      if (!is_topic_allowed_by_settings.Run(privacy_sandbox::CanonicalTopic(
              topic_and_domains.topic(), epoch.taxonomy_version()))) {
        return true;
      }
    }
  }

  return false;
}

struct StartupCalculateDecision {
  bool clear_topics_data = true;
  base::TimeDelta next_calculation_delay;
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
    return StartupCalculateDecision{
        .clear_topics_data = false,
        .next_calculation_delay = base::TimeDelta()};
  }

  // This could happen when clear-on-exit is turned on and has caused the
  // cookies to be deleted on startup, of if a topic became disallowed when
  // `browsing_topics_state` was still loading.
  bool should_clear_topics_data = ShouldClearTopicsOnStartup(
      browsing_topics_state, browsing_topics_data_accessible_since,
      is_topic_allowed_by_settings);

  base::TimeDelta presumed_next_calculation_delay =
      browsing_topics_state.next_scheduled_calculation_time() -
      base::Time::Now();

  // The scheduled calculation time was reached before the startup.
  if (presumed_next_calculation_delay <= base::TimeDelta()) {
    return StartupCalculateDecision{
        .clear_topics_data = should_clear_topics_data,
        .next_calculation_delay = base::TimeDelta()};
  }

  // This could happen if the machine time has changed since the last
  // calculation. Recalculate immediately to align with the expected schedule
  // rather than potentially stop computing for a very long time.
  if (presumed_next_calculation_delay >=
      2 * blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get()) {
    return StartupCalculateDecision{
        .clear_topics_data = should_clear_topics_data,
        .next_calculation_delay = base::TimeDelta()};
  }

  return StartupCalculateDecision{
      .clear_topics_data = should_clear_topics_data,
      .next_calculation_delay = presumed_next_calculation_delay};
}

void RecordBrowsingTopicsApiResultUkmMetrics(
    ApiAccessFailureReason failure_reason,
    content::RenderFrameHost* main_frame,
    bool is_get_topics_request) {
  // The `BrowsingTopics_DocumentBrowsingTopicsApiResult2` event is only
  // recorded for request that gets the topics.
  if (!is_get_topics_request)
    return;

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2 builder(
      main_frame->GetPageUkmSourceId());
  builder.SetFailureReason(static_cast<int64_t>(failure_reason));
  builder.Record(ukm_recorder->Get());
}

void RecordBrowsingTopicsApiResultUkmMetrics(
    const std::vector<CandidateTopic>& valid_candidate_topics,
    content::RenderFrameHost* main_frame) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2 builder(
      main_frame->GetPageUkmSourceId());

  for (size_t i = 0; i < 3u && valid_candidate_topics.size() > i; ++i) {
    const CandidateTopic& candidate_topic = valid_candidate_topics[i];

    DCHECK(candidate_topic.IsValid());

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

  builder.Record(ukm_recorder->Get());
}

}  // namespace

BrowsingTopicsServiceImpl::~BrowsingTopicsServiceImpl() = default;

BrowsingTopicsServiceImpl::BrowsingTopicsServiceImpl(
    const base::FilePath& profile_path,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    optimization_guide::PageContentAnnotationsService* annotations_service,
    TopicAccessedCallback topic_accessed_callback)
    : privacy_sandbox_settings_(privacy_sandbox_settings),
      history_service_(history_service),
      site_data_manager_(site_data_manager),
      annotations_service_(annotations_service),
      browsing_topics_state_(
          profile_path,
          base::BindOnce(
              &BrowsingTopicsServiceImpl::OnBrowsingTopicsStateLoaded,
              base::Unretained(this))),
      topic_accessed_callback_(std::move(topic_accessed_callback)) {
  DCHECK(topic_accessed_callback_);
  privacy_sandbox_settings_observation_.Observe(privacy_sandbox_settings);
  history_service_observation_.Observe(history_service);

  // Greedily request the model to be available to reduce the latency in later
  // topics calculation.
  annotations_service_->RequestAndNotifyWhenModelAvailable(
      optimization_guide::AnnotationType::kPageTopics, base::DoNothing());
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

  if (!browsing_topics_state_loaded_) {
    RecordBrowsingTopicsApiResultUkmMetrics(
        ApiAccessFailureReason::kStateNotReady, main_frame, get_topics);
    return false;
  }

  if (!privacy_sandbox_settings_->IsTopicsAllowed()) {
    RecordBrowsingTopicsApiResultUkmMetrics(
        ApiAccessFailureReason::kAccessDisallowedBySettings, main_frame,
        get_topics);
    return false;
  }

  if (!privacy_sandbox_settings_->IsTopicsAllowedForContext(
          /*top_frame_origin=*/main_frame->GetLastCommittedOrigin(),
          context_origin.GetURL())) {
    RecordBrowsingTopicsApiResultUkmMetrics(
        ApiAccessFailureReason::kAccessDisallowedBySettings, main_frame,
        get_topics);
    return false;
  }

  std::string context_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          context_origin.GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  HashedDomain hashed_context_domain = HashContextDomainForStorage(
      browsing_topics_state_.hmac_key(), context_domain);

  if (observe) {
    // Track the API usage context after the permissions check.
    BrowsingTopicsPageLoadDataTracker::GetOrCreateForPage(main_frame->GetPage())
        ->OnBrowsingTopicsApiUsed(hashed_context_domain, history_service_);
  }

  if (!get_topics)
    return true;

  std::string top_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          main_frame->GetLastCommittedOrigin().GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  std::vector<CandidateTopic> valid_candidate_topics;

  for (const EpochTopics* epoch :
       browsing_topics_state_.EpochsForSite(top_domain)) {
    CandidateTopic candidate_topic = epoch->CandidateTopicForSite(
        top_domain, hashed_context_domain, browsing_topics_state_.hmac_key());

    if (!candidate_topic.IsValid())
      continue;

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

  RecordBrowsingTopicsApiResultUkmMetrics(valid_candidate_topics, main_frame);

  for (const CandidateTopic& candidate_topic : valid_candidate_topics) {
    if (candidate_topic.should_be_filtered())
      continue;

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
        {"chrome.", base::NumberToString(
                        blink::features::kBrowsingTopicsConfigVersion.Get())});
    result_topic->model_version =
        base::NumberToString(candidate_topic.model_version());
    result_topic->taxonomy_version =
        base::NumberToString(candidate_topic.taxonomy_version());
    result_topic->version = base::StrCat({result_topic->config_version, ":",
                                          result_topic->taxonomy_version, ":",
                                          result_topic->model_version});
    topics.emplace_back(std::move(result_topic));
  }

  std::sort(topics.begin(), topics.end());

  // Remove duplicate entries.
  topics.erase(std::unique(topics.begin(), topics.end()), topics.end());

  return true;
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

    schedule_calculate_timer_.AbandonAndStop();
    CalculateBrowsingTopics();
    return;
  }

  std::move(callback).Run(GetBrowsingTopicsStateForWebUiHelper());
}

std::vector<privacy_sandbox::CanonicalTopic>
BrowsingTopicsServiceImpl::GetTopTopicsForDisplay() const {
  if (!browsing_topics_state_loaded_)
    return {};

  std::vector<privacy_sandbox::CanonicalTopic> result;

  for (const EpochTopics& epoch : browsing_topics_state_.epochs()) {
    DCHECK_LE(epoch.padded_top_topics_start_index(),
              epoch.top_topics_and_observing_domains().size());

    for (size_t i = 0; i < epoch.padded_top_topics_start_index(); ++i) {
      const TopicAndDomains& topic_and_domains =
          epoch.top_topics_and_observing_domains()[i];

      if (!topic_and_domains.IsValid())
        continue;

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

void BrowsingTopicsServiceImpl::ClearTopic(
    const privacy_sandbox::CanonicalTopic& canonical_topic) {
  if (!browsing_topics_state_loaded_)
    return;

  browsing_topics_state_.ClearTopic(canonical_topic.topic_id(),
                                    canonical_topic.taxonomy_version());
}

void BrowsingTopicsServiceImpl::ClearTopicsDataForOrigin(
    const url::Origin& origin) {
  if (!browsing_topics_state_loaded_)
    return;

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
  if (!browsing_topics_state_loaded_)
    return;

  browsing_topics_state_.ClearAllTopics();
  site_data_manager_->ExpireDataBefore(base::Time::Now());
}

std::unique_ptr<BrowsingTopicsCalculator>
BrowsingTopicsServiceImpl::CreateCalculator(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    optimization_guide::PageContentAnnotationsService* annotations_service,
    const base::circular_deque<EpochTopics>& epochs,
    BrowsingTopicsCalculator::CalculateCompletedCallback callback) {
  return std::make_unique<BrowsingTopicsCalculator>(
      privacy_sandbox_settings, history_service, site_data_manager,
      annotations_service, epochs, std::move(callback));
}

const BrowsingTopicsState& BrowsingTopicsServiceImpl::browsing_topics_state() {
  return browsing_topics_state_;
}

void BrowsingTopicsServiceImpl::ScheduleBrowsingTopicsCalculation(
    base::TimeDelta delay) {
  DCHECK(browsing_topics_state_loaded_);

  // `this` owns the timer, which is automatically cancelled on destruction, so
  // base::Unretained(this) is safe.
  schedule_calculate_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&BrowsingTopicsServiceImpl::CalculateBrowsingTopics,
                     base::Unretained(this)));
}

void BrowsingTopicsServiceImpl::CalculateBrowsingTopics() {
  DCHECK(browsing_topics_state_loaded_);

  DCHECK(!topics_calculator_);

  // `this` owns `topics_calculator_` so `topics_calculator_` should not invoke
  // the callback once it's destroyed.
  topics_calculator_ = CreateCalculator(
      privacy_sandbox_settings_, history_service_, site_data_manager_,
      annotations_service_, browsing_topics_state_.epochs(),
      base::BindOnce(
          &BrowsingTopicsServiceImpl::OnCalculateBrowsingTopicsCompleted,
          base::Unretained(this)));
}

void BrowsingTopicsServiceImpl::OnCalculateBrowsingTopicsCompleted(
    EpochTopics epoch_topics) {
  DCHECK(browsing_topics_state_loaded_);

  DCHECK(topics_calculator_);
  topics_calculator_.reset();

  browsing_topics_state_.AddEpoch(std::move(epoch_topics));
  browsing_topics_state_.UpdateNextScheduledCalculationTime();

  ScheduleBrowsingTopicsCalculation(
      blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get());

  if (!get_state_for_webui_callbacks_.empty()) {
    mojom::WebUIGetBrowsingTopicsStateResultPtr webui_state =
        GetBrowsingTopicsStateForWebUiHelper();

    for (auto& callback : get_state_for_webui_callbacks_) {
      std::move(callback).Run(webui_state->Clone());
    }

    get_state_for_webui_callbacks_.clear();
  }
}

void BrowsingTopicsServiceImpl::OnBrowsingTopicsStateLoaded() {
  DCHECK(!browsing_topics_state_loaded_);
  browsing_topics_state_loaded_ = true;

  base::Time browsing_topics_data_sccessible_since =
      privacy_sandbox_settings_->TopicsDataAccessibleSince();

  StartupCalculateDecision decision = GetStartupCalculationDecision(
      browsing_topics_state_, browsing_topics_data_sccessible_since,
      base::BindRepeating(
          &privacy_sandbox::PrivacySandboxSettings::IsTopicAllowed,
          base::Unretained(privacy_sandbox_settings_)));

  if (decision.clear_topics_data)
    browsing_topics_state_.ClearAllTopics();

  site_data_manager_->ExpireDataBefore(browsing_topics_data_sccessible_since);

  ScheduleBrowsingTopicsCalculation(decision.next_calculation_delay);
}

void BrowsingTopicsServiceImpl::Shutdown() {
  privacy_sandbox_settings_observation_.Reset();
  history_service_observation_.Reset();
}

void BrowsingTopicsServiceImpl::OnTopicsDataAccessibleSinceUpdated() {
  if (!browsing_topics_state_loaded_)
    return;

  // Here we rely on the fact that `browsing_topics_data_accessible_since` can
  // only be updated to base::Time::Now() due to data deletion. In this case, we
  // should just clear all topics.
  browsing_topics_state_.ClearAllTopics();
  site_data_manager_->ExpireDataBefore(
      privacy_sandbox_settings_->TopicsDataAccessibleSince());

  // Abort the outstanding topics calculation and restart immediately.
  if (topics_calculator_) {
    DCHECK(!schedule_calculate_timer_.IsRunning());

    topics_calculator_.reset();
    CalculateBrowsingTopics();
  }
}

void BrowsingTopicsServiceImpl::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!browsing_topics_state_loaded_)
    return;

  // Ignore invalid time_range.
  if (!deletion_info.IsAllHistory() && !deletion_info.time_range().IsValid())
    return;

  for (size_t i = 0; i < browsing_topics_state_.epochs().size(); ++i) {
    const EpochTopics& epoch_topics = browsing_topics_state_.epochs()[i];

    if (epoch_topics.empty())
      continue;

    // The typical case is assumed here. We cannot always derive the original
    // history start time, as the necessary data (e.g. its previous epoch's
    // calculation time) may have been gone.
    base::Time history_data_start_time =
        epoch_topics.calculation_time() -
        blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get();

    bool time_range_overlap =
        epoch_topics.calculation_time() >= deletion_info.time_range().begin() &&
        history_data_start_time <= deletion_info.time_range().end();

    if (time_range_overlap)
      browsing_topics_state_.ClearOneEpoch(i);
  }

  // If there's an outstanding topics calculation, abort and restart it.
  if (topics_calculator_) {
    DCHECK(!schedule_calculate_timer_.IsRunning());

    topics_calculator_.reset();
    CalculateBrowsingTopics();
  }
}

mojom::WebUIGetBrowsingTopicsStateResultPtr
BrowsingTopicsServiceImpl::GetBrowsingTopicsStateForWebUiHelper() {
  DCHECK(browsing_topics_state_loaded_);
  DCHECK(!topics_calculator_);

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
      for (const auto& domain : topic_and_domains.hashed_domains()) {
        webui_observed_by_domains.push_back(
            base::NumberToString(domain.value()));
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

  return mojom::WebUIGetBrowsingTopicsStateResult::NewBrowsingTopicsState(
      std::move(webui_state));
}

}  // namespace browsing_topics
