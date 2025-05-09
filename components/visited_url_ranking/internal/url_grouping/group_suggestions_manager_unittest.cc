// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/sessions/core/session_id.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"
#include "components/visited_url_ranking/internal/url_grouping/mock_suggestions_delegate.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/tab_metadata.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace visited_url_ranking {

constexpr char kTestUrl[] = "https://www.example1.com/";

namespace {

using ukm::builders::GroupSuggestionPromo_Suggestion;

URLVisitAggregate CreateVisitForTab(int tab_id,
                                    base::TimeDelta time_since_active,
                                    int foreground_count,
                                    bool is_selected) {
  base::Time timestamp = base::Time::Now() - time_since_active;
  auto candidate = CreateSampleURLVisitAggregate(GURL(kTestUrl), 1, timestamp,
                                                 {Fetcher::kTabModel});
  auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab_data =
      std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab_data->last_active_tab.id = tab_id;
  tab_data->recent_fg_count = foreground_count;

  auto& metadata = tab_data->last_active_tab.tab_metadata;
  metadata.tab_creation_time = timestamp - base::Hours(10);
  metadata.ukm_source_id = tab_id;
  metadata.is_currently_active = is_selected;
  return candidate;
}

void VerifyUkmResults(const std::vector<URLVisitAggregate>& candidates,
                      ukm::TestAutoSetUkmRecorder& ukm_recorder,
                      GroupSuggestion::SuggestionReason suggestion_reason,
                      GroupSuggestionsDelegate::UserResponse user_response) {
  auto ukm_entries = ukm_recorder.GetEntriesByName(
      GroupSuggestionPromo_Suggestion::kEntryName);
  ASSERT_EQ(candidates.size(), ukm_entries.size());
  for (size_t i = 0; i < candidates.size(); i++) {
    auto tab_data_it = candidates[i].fetcher_data_map.find(Fetcher::kTabModel);
    auto* tab_data =
        std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
    auto& metadata = tab_data->last_active_tab.tab_metadata;
    const auto& ukm_entry = ukm_entries[i];
    ukm_recorder.ExpectEntryMetric(
        ukm_entry, GroupSuggestionPromo_Suggestion::kIsActiveTabName,
        metadata.is_currently_active);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry, GroupSuggestionPromo_Suggestion::kSecondsSinceLastActiveName,
        ukm::GetExponentialBucketMinForFineUserTiming(
            (base::Time::Now() - tab_data->last_active).InSeconds()));
    ukm_recorder.ExpectEntryMetric(
        ukm_entry, GroupSuggestionPromo_Suggestion::kRecentForegroundCountName,
        tab_data->recent_fg_count);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry, GroupSuggestionPromo_Suggestion::kGroupSuggestionTypeName,
        static_cast<int64_t>(suggestion_reason));
    ukm_recorder.ExpectEntryMetric(
        ukm_entry, GroupSuggestionPromo_Suggestion::kUserResponseTypeName,
        static_cast<int64_t>(user_response));
    EXPECT_EQ(*ukm::TestAutoSetUkmRecorder::GetEntryMetric(
                  ukm_entries[0],
                  GroupSuggestionPromo_Suggestion::kGroupSuggestionIDName),
              *ukm::TestAutoSetUkmRecorder::GetEntryMetric(
                  ukm_entry,
                  GroupSuggestionPromo_Suggestion::kGroupSuggestionIDName));
  }
}

TabMetadata& GetTabMetadata(URLVisitAggregate& visit) {
  auto tab_data_it = visit.fetcher_data_map.find(Fetcher::kTabModel);
  return std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second)
      ->last_active_tab.tab_metadata;
}
}  // namespace

class GroupSuggestionsManagerTest : public testing::Test {
 public:
  GroupSuggestionsManagerTest() = default;
  ~GroupSuggestionsManagerTest() override = default;

  void SetUp() override {
    Test::SetUp();
    auto* registry = pref_service_.registry();
    GroupSuggestionsServiceImpl::RegisterProfilePrefs(registry);
    mock_ranking_service_ = std::make_unique<MockVisitedURLRankingService>();
    suggestions_manager_ = std::make_unique<GroupSuggestionsManager>(
        mock_ranking_service_.get(), &pref_service_);
  }

  void TearDown() override {
    suggestions_manager_.reset();
    Test::TearDown();
  }

 protected:
  void OnSuggestionResult(
      GroupSuggestion shown_suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs,
      GroupSuggestionsDelegate::UserResponseMetadata user_response) {
    suggestions_manager_->OnSuggestionResult(std::move(shown_suggestion),
                                             inputs, std::move(user_response));
  }

  void OnFinishComputeSuggestions(
      const GroupSuggestionsService::Scope& scope,
      GroupingHeuristics::SuggestionsResult result) {
    suggestions_manager_->OnFinishComputeSuggestions(scope, std::move(result));
  }

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList features_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MockVisitedURLRankingService> mock_ranking_service_;
  std::unique_ptr<GroupSuggestionsManager> suggestions_manager_;
};

TEST_F(GroupSuggestionsManagerTest, RegisterDelegate) {
  MockGroupSuggestionsDelegate delegate;
  GroupSuggestionsService::Scope scope{.tab_session_id =
                                           SessionID::NewUnique()};
  suggestions_manager_->UnregisterDelegate(nullptr);
  suggestions_manager_->UnregisterDelegate(&delegate);

  suggestions_manager_->RegisterDelegate(&delegate, scope);
  suggestions_manager_->RegisterDelegate(&delegate, scope);

  suggestions_manager_->UnregisterDelegate(&delegate);
  suggestions_manager_->UnregisterDelegate(&delegate);
}

TEST_F(GroupSuggestionsManagerTest, TriggerSuggestions) {
  // Reset manager so that computation delay is reset.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"consecutive_computation_delay_sec", "5"}});
  suggestions_manager_.reset();
  suggestions_manager_ = std::make_unique<GroupSuggestionsManager>(
      mock_ranking_service_.get(), &pref_service_);

  GroupSuggestionsService::Scope scope{.tab_session_id =
                                           SessionID::NewUnique()};
  GroupSuggestionsService::Scope scope1{.tab_session_id =
                                            SessionID::NewUnique()};

  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _));
  suggestions_manager_->MaybeTriggerSuggestions(scope);

  task_env_.FastForwardBy(base::Seconds(5));
  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _));
  suggestions_manager_->MaybeTriggerSuggestions(scope);

  task_env_.FastForwardBy(base::Seconds(5));
  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _));
  suggestions_manager_->MaybeTriggerSuggestions(scope1);

  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _)).Times(0);
  suggestions_manager_->MaybeTriggerSuggestions(scope);
}

TEST_F(GroupSuggestionsManagerTest, OnSuggestionResult_RecordUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Prepare suggestion.
  GroupSuggestion shown_suggestion;
  shown_suggestion.tab_ids = {111, 222, 333};
  shown_suggestion.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  // Prepare input.
  std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs;
  std::vector<URLVisitAggregate> candidates = {};
  candidates.push_back(CreateVisitForTab(111, base::Seconds(60), 2, true));
  candidates.push_back(CreateVisitForTab(222, base::Seconds(250), 1, false));
  candidates.push_back(CreateVisitForTab(333, base::Seconds(350), 1, false));
  for (const auto& candidate : candidates) {
    inputs.push_back(AsInputContext(kSuggestionsPredictionSchema, candidate));
  }
  // Prepare user response.
  GroupSuggestionsDelegate::UserResponseMetadata response;
  response.user_response = GroupSuggestionsDelegate::UserResponse::kAccepted;

  OnSuggestionResult(std::move(shown_suggestion), std::move(inputs),
                     std::move(response));

  VerifyUkmResults(std::move(candidates), ukm_recorder,
                   GroupSuggestion::SuggestionReason::kRecentlyOpened,
                   GroupSuggestionsDelegate::UserResponse::kAccepted);
}

TEST_F(GroupSuggestionsManagerTest,
       OnSuggestionResult_OnlyRecordUKMForSuggestion) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Prepare suggestion.
  GroupSuggestion shown_suggestion;
  shown_suggestion.tab_ids = {111, 222, 333, 444};
  shown_suggestion.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;

  // Mock that there are more tabs in candidates, but some are should not be
  // included in the UKM.
  std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs;
  std::vector<URLVisitAggregate> candidates = {};
  candidates.push_back(CreateVisitForTab(111, base::Seconds(60), 2, true));
  candidates.push_back(CreateVisitForTab(222, base::Seconds(250), 1, false));
  candidates.push_back(CreateVisitForTab(333, base::Seconds(350), 1, false));
  // Should not be included because the ukm_source_id is invalid.
  candidates.push_back(CreateVisitForTab(444, base::Seconds(100), 1, false));
  GetTabMetadata(candidates[3]).ukm_source_id = ukm::kInvalidSourceId;
  // Should not be included because it's not part of the suggestion.
  candidates.push_back(CreateVisitForTab(555, base::Seconds(200), 1, false));
  for (const auto& candidate : candidates) {
    inputs.push_back(AsInputContext(kSuggestionsPredictionSchema, candidate));
  }
  // Prepare user response.
  GroupSuggestionsDelegate::UserResponseMetadata response;
  response.user_response = GroupSuggestionsDelegate::UserResponse::kAccepted;

  OnSuggestionResult(std::move(shown_suggestion), std::move(inputs),
                     std::move(response));

  // Remove last two tabs that should not be included from expectation.
  candidates.pop_back();
  candidates.pop_back();
  VerifyUkmResults(std::move(candidates), ukm_recorder,
                   GroupSuggestion::SuggestionReason::kRecentlyOpened,
                   GroupSuggestionsDelegate::UserResponse::kAccepted);
}

TEST_F(GroupSuggestionsManagerTest,
       OnFinishComputeSuggestions_RecordHistograms) {
  base::HistogramTester histogram_tester;
  // Prepare suggestion.
  GroupSuggestion shown_suggestion;
  shown_suggestion.tab_ids = {111, 222, 333};
  shown_suggestion.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  // Prepare input.
  // Tab indexes are 1, 4, 9. Max gap is 5 and average gap is 4.
  std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs;
  std::vector<URLVisitAggregate> candidates = {};
  candidates.push_back(CreateVisitForTab(111, base::Seconds(60), 2, true));
  GetTabMetadata(candidates[0]).tab_model_index = 1;
  GetTabMetadata(candidates[0]).is_last_tab_in_tab_model = false;
  candidates.push_back(CreateVisitForTab(222, base::Seconds(250), 1, false));
  GetTabMetadata(candidates[1]).tab_model_index = 4;
  GetTabMetadata(candidates[1]).is_last_tab_in_tab_model = false;
  candidates.push_back(CreateVisitForTab(333, base::Seconds(350), 1, false));
  GetTabMetadata(candidates[2]).tab_model_index = 9;
  GetTabMetadata(candidates[2]).is_last_tab_in_tab_model = true;
  for (const auto& candidate : candidates) {
    inputs.push_back(AsInputContext(kSuggestionsPredictionSchema, candidate));
  }

  GroupingHeuristics::SuggestionsResult result;
  GroupSuggestions suggestions;
  suggestions.suggestions.push_back(std::move(shown_suggestion));
  result.suggestions = std::move(suggestions);
  result.inputs = std::move(inputs);

  OnFinishComputeSuggestions(GroupSuggestionsService::Scope(),
                             std::move(result));

  histogram_tester.ExpectBucketCount("GroupSuggestionsService.SuggestionsCount",
                                     1, 1);
  histogram_tester.ExpectBucketCount(
      "GroupSuggestionsService.SuggestionsCountAfterThrottling", 1, 1);
  histogram_tester.ExpectBucketCount(
      "GroupSuggestionsService.TopSuggestionReason",
      shown_suggestion.suggestion_reason, 1);
  histogram_tester.ExpectBucketCount(
      "GroupSuggestionsService.TopSuggestionTabCount", 3, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {"GroupSuggestionsService.TopSuggestionTabCount.",
           GetSuggestionReasonString(shown_suggestion.suggestion_reason)}),
      3, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {"GroupSuggestionsService.TopSuggestionContainsLastTab.",
           GetSuggestionReasonString(shown_suggestion.suggestion_reason)}),
      1, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {"GroupSuggestionsService.TopSuggestionTabIndexMaxGap.",
           GetSuggestionReasonString(shown_suggestion.suggestion_reason)}),
      5, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {"GroupSuggestionsService.TopSuggestionTabIndexAverageGap.",
           GetSuggestionReasonString(shown_suggestion.suggestion_reason)}),
      4, 1);
}
}  // namespace visited_url_ranking
