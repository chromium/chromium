// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/visited_url_ranking/internal/url_grouping/mock_suggestions_delegate.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/tab_metadata.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {
namespace {

using ::testing::_;
using ::testing::ElementsAre;

constexpr char kTestUrl1[] = "https://www.example1.com/";
constexpr char kTestUrl2[] = "https://www.example2.com/";
constexpr char kTestUrl3[] = "https://www.example3.com/";
constexpr char kTestUrl4[] = "https://www.example4.com/";
constexpr char kTestUrl5[] = "https://www.example5.com/";
constexpr char kTestUrl6[] = "https://www.example6.com/";
constexpr char kTestUrl7[] = "https://www.example7.com/";
constexpr char kTestUrl8[] = "https://www.example8.com/";
constexpr char kTestUrl9[] = "https://www.example9.com/";
constexpr char kTestUrl10[] = "https://www.example10.com/";
constexpr char kTestUrl11[] = "https://www.example11.com/";
constexpr char kTestUrl12[] = "https://www.example12.com/";

URLVisitAggregate CreateVisitForTab(base::TimeDelta time_since_active,
                                    int tab_id,
                                    const GURL& url) {
  base::Time timestamp = base::Time::Now() - time_since_active;
  auto candidate =
      CreateSampleURLVisitAggregate(url, 1, timestamp, {Fetcher::kTabModel});
  auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab->last_active_tab.id = tab_id;

  history::AnnotatedVisit annotated_visit = GenerateSampleAnnotatedVisit(
      /*visit_id=*/1, /*page_title=*/u"Test Title", GURL(),
      /*has_url_keyed_image=*/true, /*originator_cache_guid=*/"",
      /*visibility_score=*/0.9, /*categories=*/{}, base::Time());
  candidate.fetcher_data_map.emplace(
      Fetcher::kHistory,
      URLVisitAggregate::HistoryData(std::move(annotated_visit)));

  return candidate;
}

TabMetadata& GetTabMetadata(URLVisitAggregate& visit) {
  auto tab_data_it = visit.fetcher_data_map.find(Fetcher::kTabModel);
  return std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second)
      ->last_active_tab.tab_metadata;
}

class MockTabEventsVisitTransformer : public TabEventsVisitTransformer {
 public:
  MockTabEventsVisitTransformer() = default;
  MockTabEventsVisitTransformer(const MockTabEventsVisitTransformer&) = delete;
  MockTabEventsVisitTransformer& operator=(
      const MockTabEventsVisitTransformer&) = delete;
  ~MockTabEventsVisitTransformer() override = default;

  MOCK_METHOD3(Transform,
               void(std::vector<URLVisitAggregate> aggregates,
                    const FetchOptions& options,
                    OnTransformCallback callback));
};

class GroupSuggestionsServiceImplTest : public testing::Test {
 public:
  GroupSuggestionsServiceImplTest() = default;
  ~GroupSuggestionsServiceImplTest() override = default;

  void SetUp() override {
    Test::SetUp();
    features_.InitAndEnableFeatureWithParameters(
        features::kGroupSuggestionService,
        {{"group_suggestion_enable_recently_opened", "true"}});
    auto* registry = pref_service_.registry();
    GroupSuggestionsServiceImpl::RegisterProfilePrefs(registry);
    mock_ranking_service_ = std::make_unique<MockVisitedURLRankingService>();
    mock_transformer_ = std::make_unique<MockTabEventsVisitTransformer>();
    mock_delegate_ = std::make_unique<MockGroupSuggestionsDelegate>();
    suggestions_service_ = std::make_unique<GroupSuggestionsServiceImpl>(
        mock_ranking_service_.get(), mock_transformer_.get(), &pref_service_);
  }

  void TearDown() override {
    suggestions_service_.reset();
    mock_delegate_.reset();
    mock_transformer_.reset();
    mock_ranking_service_.reset();
    Test::TearDown();
  }

  std::vector<URLVisitAggregate> GetSampleCandidates() {
    std::vector<URLVisitAggregate> candidates;
    // Add 5 tabs within 600 seconds and one over 600. The first 5 tabs should
    // be grouped.
    candidates.push_back(
        CreateVisitForTab(base::Seconds(60), 111, GURL(kTestUrl1)));
    GetTabMetadata(candidates[0]).is_currently_active = true;
    candidates.push_back(
        CreateVisitForTab(base::Seconds(250), 112, GURL(kTestUrl2)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(300), 114, GURL(kTestUrl3)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(500), 115, GURL(kTestUrl4)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(500), 116, GURL(kTestUrl5)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(800), 117, GURL(kTestUrl6)));
    return candidates;
  }

  std::vector<URLVisitAggregate> GetNonOverlappingCandidates() {
    std::vector<URLVisitAggregate> candidates;
    // 5 tabs with new IDs.
    candidates.push_back(
        CreateVisitForTab(base::Seconds(60), 11, GURL(kTestUrl7)));
    GetTabMetadata(candidates[0]).is_currently_active = true;
    candidates.push_back(
        CreateVisitForTab(base::Seconds(250), 12, GURL(kTestUrl8)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(300), 14, GURL(kTestUrl9)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(500), 15, GURL(kTestUrl10)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(500), 16, GURL(kTestUrl11)));
    candidates.push_back(
        CreateVisitForTab(base::Seconds(800), 17, GURL(kTestUrl12)));
    return candidates;
  }

  struct SuggestionTriggerData {
    SuggestionResponseCallback callback;
    GroupSuggestion suggestion;
  };

  SuggestionTriggerData TriggerSuggestions(
      std::vector<URLVisitAggregate> candidates) {
    base::RunLoop wait_for_compute;
    suggestions_service_->group_suggestions_manager_for_testing()
        ->set_suggestion_computed_callback_for_testing(
            wait_for_compute.QuitClosure());
    VisitedURLRankingService::GetURLVisitAggregatesCallback fetch_callback;
    ON_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _))
        .WillByDefault(MoveArg<1>(&fetch_callback));

    task_environment_.AdvanceClock(base::Seconds(10));
    suggestions_service_->GetTabEventTracker()->DidAddTab(1, 0);

    SuggestionTriggerData data;
    ON_CALL(*mock_delegate_, ShowSuggestion(_, _))
        .WillByDefault([&data](const GroupSuggestions& group_suggestions,
                               SuggestionResponseCallback response_callback) {
          data.callback = std::move(response_callback);
          data.suggestion = group_suggestions.suggestions.front().DeepCopy();
        });
    std::move(fetch_callback)
        .Run(ResultStatus::kSuccess, URLVisitsMetadata{},
             std::move(candidates));
    wait_for_compute.Run();
    suggestions_service_->group_suggestions_manager_for_testing()
        ->set_suggestion_computed_callback_for_testing({});
    return data;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList features_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MockVisitedURLRankingService> mock_ranking_service_;
  std::unique_ptr<MockTabEventsVisitTransformer> mock_transformer_;
  std::unique_ptr<MockGroupSuggestionsDelegate> mock_delegate_;
  std::unique_ptr<GroupSuggestionsServiceImpl> suggestions_service_;
};

TEST_F(GroupSuggestionsServiceImplTest, EndToEnd) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _))
      .WillOnce([](const GroupSuggestions& group_suggestions,
                   SuggestionResponseCallback response_callback) {
        ASSERT_EQ(1u, group_suggestions.suggestions.size());
        const GroupSuggestion& suggestion =
            group_suggestions.suggestions.front();
        EXPECT_EQ(suggestion.suggestion_reason,
                  GroupSuggestion::SuggestionReason::kRecentlyOpened);
        EXPECT_FALSE(suggestion.suggestion_id.is_null());
        EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 114, 115, 116));
        EXPECT_FALSE(suggestion.promo_contents.empty());
        EXPECT_FALSE(suggestion.promo_header.empty());
      });
  TriggerSuggestions(GetSampleCandidates());
}

TEST_F(GroupSuggestionsServiceImplTest, NoRepeatedSuggestions) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _));
  auto trigger_data = TriggerSuggestions(GetSampleCandidates());
  UserResponseMetadata response;
  response.user_response = UserResponse::kRejected;
  response.suggestion_id = trigger_data.suggestion.suggestion_id;
  std::move(trigger_data.callback).Run(response);

  // Triggering suggestions again should not show since its duplicate:

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _)).Times(0);
  TriggerSuggestions(GetSampleCandidates());

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _)).Times(0);
  TriggerSuggestions(GetSampleCandidates());

  auto candidates = GetNonOverlappingCandidates();
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _)).Times(1);
  TriggerSuggestions(std::move(candidates));
}

TEST_F(GroupSuggestionsServiceImplTest, GroupedTabsNotIncluded) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  auto candidates = GetSampleCandidates();

  // Set group for tab ID 115.
  auto tab_data_it = candidates[3].fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab->last_active_tab.tab_metadata.local_tab_group_id =
      base::Token::CreateRandom();

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _))
      .WillOnce([](const GroupSuggestions& group_suggestions,
                   SuggestionResponseCallback response_callback) {
        ASSERT_EQ(1u, group_suggestions.suggestions.size());
        const GroupSuggestion& suggestion =
            group_suggestions.suggestions.front();
        // 115 not included as its part of a group.
        EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 114, 116));
      });

  TriggerSuggestions(std::move(candidates));
}

TEST_F(GroupSuggestionsServiceImplTest, GetCachedSuggestions_EmptyCache) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  std::optional<CachedSuggestions> cached_suggestions =
      suggestions_service_->GetCachedSuggestions(
          GroupSuggestionsService::Scope());
  EXPECT_FALSE(cached_suggestions.has_value());
}

TEST_F(GroupSuggestionsServiceImplTest, GetCachedSuggestions_PopulatedCache) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _));
  auto trigger_data = TriggerSuggestions(GetSampleCandidates());

  std::optional<CachedSuggestions> cached_suggestions_optional =
      suggestions_service_->GetCachedSuggestions(
          GroupSuggestionsService::Scope());
  ASSERT_TRUE(cached_suggestions_optional.has_value());

  const auto& cached_group_suggestions =
      cached_suggestions_optional->suggestions;
  ASSERT_EQ(1u, cached_group_suggestions.suggestions.size());
  const GroupSuggestion& cached_suggestion =
      cached_group_suggestions.suggestions.front();

  EXPECT_EQ(cached_suggestion.suggestion_reason,
            GroupSuggestion::SuggestionReason::kRecentlyOpened);
  // Tab IDs should match the original suggestion that was cached.
  // The `GetSampleCandidates` produces a suggestion with these tabs.
  EXPECT_THAT(cached_suggestion.tab_ids, ElementsAre(111, 112, 114, 115, 116));
  EXPECT_FALSE(cached_suggestions_optional->response_callback.is_null());

  // Verify cache is not cleared by calling again.
  std::optional<CachedSuggestions> cached_suggestions_optional_again =
      suggestions_service_->GetCachedSuggestions(
          GroupSuggestionsService::Scope());
  ASSERT_TRUE(cached_suggestions_optional_again.has_value());
  const auto& cached_group_suggestions_again =
      cached_suggestions_optional_again->suggestions;
  ASSERT_EQ(1u, cached_group_suggestions_again.suggestions.size());
  const GroupSuggestion& cached_suggestion_again =
      cached_group_suggestions_again.suggestions.front();
  EXPECT_EQ(cached_suggestion_again.suggestion_reason,
            GroupSuggestion::SuggestionReason::kRecentlyOpened);
  EXPECT_THAT(cached_suggestion_again.tab_ids,
              ElementsAre(111, 112, 114, 115, 116));
}

}  // namespace
}  // namespace visited_url_ranking
