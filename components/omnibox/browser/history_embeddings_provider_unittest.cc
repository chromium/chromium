// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_embeddings_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/history_embeddings/mock_history_embeddings_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"
#include "components/search_engines/template_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using enum history_embeddings::ComputeAnswerStatus;

namespace {
AutocompleteInput CreateAutocompleteInput(const std::u16string input) {
  return {input, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier()};
}

history_embeddings::ScoredUrlRow CreateScoredUrlRow(
    float score,
    const std::string& url,
    const std::u16string& title) {
  history_embeddings::ScoredUrlRow scored_url_row(
      history_embeddings::ScoredUrl(0, 0, {}, score));
  scored_url_row.row = history::URLRow{GURL{url}};
  scored_url_row.row.set_title(title);
  scored_url_row.passages_embeddings.url_passages.passages.add_passages(
      "passage");
  scored_url_row.passages_embeddings.url_embeddings.embeddings.emplace_back(
      std::vector<float>(768, 1.0f));
  scored_url_row.scores.push_back(score);
  return scored_url_row;
}

history_embeddings::SearchResult CreateSearchResult(
    const std::string& query,
    const std::u16string& title) {
  history_embeddings::SearchResult result;
  result.query = query;
  result.scored_url_rows = {
      CreateScoredUrlRow(.5, "https://url.com/", title),
  };
  return result;
}

TemplateURL CreateTemplateUrl() {
  TemplateURLData template_url_data;
  template_url_data.SetShortName(u"shortname");
  template_url_data.SetKeyword(u"keyword");
  template_url_data.SetURL("https://url.com");
  return TemplateURL{template_url_data};
}
}  // namespace

class FakeHistoryEmbeddingsProvider : public HistoryEmbeddingsProvider {
 public:
  using HistoryEmbeddingsProvider::HistoryEmbeddingsProvider;
  using HistoryEmbeddingsProvider::OnReceivedSearchResult;

  using HistoryEmbeddingsProvider::done_;
  using HistoryEmbeddingsProvider::input_;
  using HistoryEmbeddingsProvider::matches_;
  using HistoryEmbeddingsProvider::starter_pack_engine_;

 private:
  ~FakeHistoryEmbeddingsProvider() override = default;
};

class HistoryEmbeddingsProviderTest : public testing::Test,
                                      public AutocompleteProviderListener {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    CHECK(history_dir_.CreateUniqueTempDir());
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    client_->set_history_service(
        history::CreateHistoryService(history_dir_.GetPath(), true));
    client_->set_history_embeddings_service(
        std::make_unique<testing::NiceMock<
            history_embeddings::MockHistoryEmbeddingsService>>(
            client_->GetHistoryService()));
    history_embeddings_service_ = static_cast<
        testing::NiceMock<history_embeddings::MockHistoryEmbeddingsService>*>(
        client_->GetHistoryEmbeddingsService());
    history_embeddings_provider_ =
        new FakeHistoryEmbeddingsProvider(client_.get(), this);

    // When `Search()` is called, pushes a callback to `search_callbacks_` that
    // can be ran to simulate `Search()` responding asyncly.
    ON_CALL(*history_embeddings_service_,
            Search(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillByDefault(
            [&](history_embeddings::SearchResult* previous_search_result,
                std::string query, std::optional<base::Time> time_range_start,
                size_t count,
                history_embeddings::SearchResultCallback callback) {
              search_callbacks_.push_back(base::BindOnce(
                  [](history_embeddings::SearchResultCallback callback,
                     std::string query, std::u16string response) {
                    std::move(callback).Run(
                        CreateSearchResult(query, response));
                  },
                  std::move(callback)));
              return history_embeddings::SearchResult();
            });
  }

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {
    last_update_matches_ = history_embeddings_provider_->matches_;
  }

  base::ScopedTempDir history_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  raw_ptr<testing::NiceMock<history_embeddings::MockHistoryEmbeddingsService>>
      history_embeddings_service_;
  scoped_refptr<FakeHistoryEmbeddingsProvider> history_embeddings_provider_;
  // Callbacks created when `Search()` is called. Running a callback with
  // `(query, response)` will simulate `Search()` responding with a result with
  // query `query` and 1 row  with title `response`.
  std::vector<base::OnceCallback<void(std::string, std::u16string)>>
      search_callbacks_;
  // The last set of matches the provider gave the autocomplete controller.
  ACMatches last_update_matches_;
};

TEST_F(HistoryEmbeddingsProviderTest, Start) {
  OmniboxTriggeredFeatureService* trigger_service =
      client_->GetOmniboxTriggeredFeatureService();
  OmniboxTriggeredFeatureService::Feature trigger_feature =
      metrics::OmniboxEventProto_Feature_HISTORY_EMBEDDINGS_FEATURE;

  AutocompleteInput short_input = CreateAutocompleteInput(u"query");
  AutocompleteInput sync_long_input =
      CreateAutocompleteInput(u"query query query");
  sync_long_input.set_omit_asynchronous_matches(true);
  AutocompleteInput long_input = CreateAutocompleteInput(u"query query query");

  // When the feature is disabled, should early exit.
  EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      *history_embeddings_service_,
      Search(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  history_embeddings_provider_->Start(long_input, false);
  EXPECT_FALSE(trigger_service->GetFeatureTriggeredInSession(trigger_feature));

  // Short queries should be blocked.
  base::test::ScopedFeatureList enabled_feature;
  enabled_feature.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings,
        {{history_embeddings::kSearchQueryMinimumWordCount.name, "3"}}},
#if BUILDFLAG(IS_CHROMEOS)
       {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
      },
      /*disabled_features=*/{});
  EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(
      *history_embeddings_service_,
      Search(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  history_embeddings_provider_->Start(short_input, false);
  EXPECT_FALSE(trigger_service->GetFeatureTriggeredInSession(trigger_feature));
  trigger_service->ResetSession();

  // Sync queries should be blocked.
  EXPECT_CALL(
      *history_embeddings_service_,
      Search(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  history_embeddings_provider_->Start(sync_long_input, false);
  EXPECT_FALSE(trigger_service->GetFeatureTriggeredInSession(trigger_feature));
  trigger_service->ResetSession();

  // Long queries should pass.
  EXPECT_CALL(*history_embeddings_service_,
              Search(testing::_, "query query query",
                     std::optional<base::Time>{}, 3u, testing::_))
      .Times(1);
  history_embeddings_provider_->Start(long_input, false);
  EXPECT_TRUE(trigger_service->GetFeatureTriggeredInSession(trigger_feature));
}

TEST_F(HistoryEmbeddingsProviderTest, Start_MultipleSequentialSearches) {
  EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
      .WillRepeatedly(testing::Return(true));

  // Start 1st search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"1 1 1"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  // Check results are populated when 1st search responds.
  std::move(search_callbacks_[0]).Run("1 1 1", u"1");
  EXPECT_THAT(last_update_matches_,
              testing::ElementsAre(
                  testing::Field(&AutocompleteMatch::description, u"1")));

  // Start 2nd search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"2 2 2"), false);
  EXPECT_THAT(last_update_matches_,
              testing::ElementsAre(
                  testing::Field(&AutocompleteMatch::description, u"1")));

  // Check results are populated when 2nd search responds.
  std::move(search_callbacks_[1]).Run("2 2 2", u"2");
  EXPECT_THAT(last_update_matches_,
              testing::ElementsAre(
                  testing::Field(&AutocompleteMatch::description, u"2")));
}

TEST_F(HistoryEmbeddingsProviderTest, Start_MultipleParallelSearches) {
  EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
      .WillRepeatedly(testing::Return(true));

  // Start 1st search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"1 1 1"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  // Start 2nd search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"2 2 2"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  // Check results are not populated when 1st search responds.
  std::move(search_callbacks_[0]).Run("1 1 1", u"1");
  EXPECT_TRUE(last_update_matches_.empty());

  // Check results are populated when 2nd search responds.
  std::move(search_callbacks_[1]).Run("2 2 2", u"2");
  EXPECT_THAT(last_update_matches_,
              testing::ElementsAre(
                  testing::Field(&AutocompleteMatch::description, u"2")));
}

TEST_F(HistoryEmbeddingsProviderTest,
       Start_MultipleParallelSearchesWithSameQuery) {
  EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
      .WillRepeatedly(testing::Return(true));

  // Start 1st search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"1 1 1"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  // Start 2nd search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"1 1 1"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  // Check results are populated when 1st search responds. Even though the
  // provider usually only cares about the most recent `Search()`, since the
  // input didn't change, it can use the 1st `Search()`.
  std::move(search_callbacks_[0]).Run("1 1 1", u"1");
  EXPECT_THAT(last_update_matches_,
              testing::ElementsAre(
                  testing::Field(&AutocompleteMatch::description, u"1")));

  // Check results aren't replaced when 2nd search responds. The provider
  // already reported `done_ = true` and it would break autocompletion to send
  // an update after doing so.
  std::move(search_callbacks_[1]).Run("1 1 1", u"2");
  EXPECT_THAT(last_update_matches_,
              testing::ElementsAre(
                  testing::Field(&AutocompleteMatch::description, u"1")));
}

TEST_F(HistoryEmbeddingsProviderTest,
       Start_MultipleParallelSearchesWithIneligibleQuery) {
  base::test::ScopedFeatureList enabled_feature;
  enabled_feature.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings,
        {{history_embeddings::kSearchQueryMinimumWordCount.name, "3"}}},
#if BUILDFLAG(IS_CHROMEOS)
       {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
      },
      /*disabled_features=*/{});
  EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
      .WillRepeatedly(testing::Return(true));

  // Start 1st search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"1 1 1"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  // Start 2nd search. It's too short.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"2 2"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  // Ensure a stale search doesn't populate matches.
  std::move(search_callbacks_[0]).Run("1 1 1", u"1");
  EXPECT_TRUE(last_update_matches_.empty());

  // Ensure a 2nd search wasn't made.
  EXPECT_EQ(search_callbacks_.size(), 1u);
}

TEST_F(HistoryEmbeddingsProviderTest, Start_Stop_SearchCompletesAfterStop) {
  EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
      .WillRepeatedly(testing::Return(true));

  // Start search.
  history_embeddings_provider_->Start(CreateAutocompleteInput(u"1 1 1"), false);
  EXPECT_TRUE(last_update_matches_.empty());

  history_embeddings_provider_->Stop(false, false);

  // Results returned after `Stop()` should be discarded.
  std::move(search_callbacks_[0]).Run("1 1 1", u"1");
  EXPECT_TRUE(last_update_matches_.empty());
}

TEST_F(HistoryEmbeddingsProviderTest, Stop) {
  history_embeddings_provider_->done_ = false;

  // TODO(crbug.com/364303536) Temporarily allow history embeddings provider to
  //   ignore `Stop()`.
  history_embeddings_provider_->Stop(false, true);
  EXPECT_FALSE(history_embeddings_provider_->done_);

  history_embeddings_provider_->Stop(false, false);
  EXPECT_TRUE(history_embeddings_provider_->done_);
}

TEST_F(HistoryEmbeddingsProviderTest, DeleteMatch) {
  AutocompleteMatch match(history_embeddings_provider_.get(), 1000, true,
                          AutocompleteMatchType::HISTORY_EMBEDDINGS);
  match.destination_url = GURL{"https://en.wikipedia.org/wiki/Matenadaran"};
  history_embeddings_provider_->matches_.push_back(match);
  history_embeddings_provider_->DeleteMatch(match);
  EXPECT_TRUE(history_embeddings_provider_->matches_.empty());
}

TEST_F(HistoryEmbeddingsProviderTest,
       OnReceivedSearchResult_CreatesAutocompleteMatches) {
  history_embeddings::SearchResult result;
  result.query = "query";
  result.scored_url_rows = {
      CreateScoredUrlRow(.5, "https://url.com/", u"title"),
  };
  history_embeddings_provider_->done_ = false;
  history_embeddings_provider_->input_ = CreateAutocompleteInput(u"query");
  history_embeddings_provider_->OnReceivedSearchResult(std::move(result));

  EXPECT_TRUE(history_embeddings_provider_->done_);
  ASSERT_EQ(history_embeddings_provider_->matches_.size(), 1u);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].provider.get(),
            history_embeddings_provider_.get());
  EXPECT_EQ(history_embeddings_provider_->matches_[0].relevance, 500);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].deletable, true);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].type,
            AutocompleteMatchType::HISTORY_EMBEDDINGS);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].destination_url.spec(),
            "https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].description, u"title");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].contents,
            u"https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].keyword, u"");
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      history_embeddings_provider_->matches_[0].transition,
      ui::PAGE_TRANSITION_TYPED));
}

TEST_F(HistoryEmbeddingsProviderTest,
       OnReceivedSearchResult_CreatesScopedAutocompleteMatches) {
  // Verifies autocomplete match is created correctly when the user is in
  // keyword mode.
  TemplateURL template_url = CreateTemplateUrl();
  history_embeddings_provider_->starter_pack_engine_ = &template_url;

  history_embeddings::SearchResult result;
  result.query = "query";
  result.scored_url_rows = {
      CreateScoredUrlRow(.5, "https://url.com/", u"title"),
  };
  history_embeddings_provider_->done_ = false;
  history_embeddings_provider_->input_ = CreateAutocompleteInput(u"query");
  history_embeddings_provider_->OnReceivedSearchResult(std::move(result));

  EXPECT_TRUE(history_embeddings_provider_->done_);
  ASSERT_EQ(history_embeddings_provider_->matches_.size(), 1u);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].provider.get(),
            history_embeddings_provider_.get());
  EXPECT_EQ(history_embeddings_provider_->matches_[0].relevance, 500);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].deletable, true);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].type,
            AutocompleteMatchType::HISTORY_EMBEDDINGS);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].destination_url.spec(),
            "https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].description, u"title");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].contents,
            u"https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].keyword, u"keyword");
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      history_embeddings_provider_->matches_[0].transition,
      ui::PAGE_TRANSITION_KEYWORD));
}

TEST_F(HistoryEmbeddingsProviderTest,
       OnReceivedSearchResult_CreatesScopedAutocompleteAnswerMatches) {
  base::test::ScopedFeatureList enabled_feature;
  enabled_feature.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings,
        {{history_embeddings::kAnswersInOmniboxScoped.name, "true"},
         {history_embeddings::kTrimAfterHostInResults.name, "true"}}},
#if BUILDFLAG(IS_CHROMEOS)
       {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
      },
      /*disabled_features=*/{});

  TemplateURL template_url = CreateTemplateUrl();
  history_embeddings_provider_->starter_pack_engine_ = &template_url;

  // 1st response with status `kUnspecified`.
  history_embeddings::SearchResult result;
  result.query = "query";
  result.scored_url_rows = {
      CreateScoredUrlRow(.75, "https://url1.com/", u"title"),
      CreateScoredUrlRow(.50, "https://url2.com/path?key=value", u"title"),
      CreateScoredUrlRow(.25, "https://url3.com/", u"title"),
  };
  base::Time time;
  EXPECT_TRUE(base::Time::FromLocalExploded(
      {.year = 2025, .month = 3, .day_of_month = 23}, &time));
  result.scored_url_rows[1].row.set_last_visit(time);

  history_embeddings_provider_->done_ = false;
  history_embeddings_provider_->input_ = CreateAutocompleteInput(u"query");
  history_embeddings_provider_->input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  history_embeddings_provider_->OnReceivedSearchResult(result.Clone());

  // Set up expected matches.
  auto expected_match_1 =
      AllOf(Field(&AutocompleteMatch::relevance, 750),
            Field(&AutocompleteMatch::type,
                  AutocompleteMatchType::HISTORY_EMBEDDINGS),
            Field(&AutocompleteMatch::destination_url, "https://url1.com/"));
  auto expected_match_2 =
      AllOf(Field(&AutocompleteMatch::relevance, 500),
            Field(&AutocompleteMatch::type,
                  AutocompleteMatchType::HISTORY_EMBEDDINGS),
            Field(&AutocompleteMatch::destination_url,
                  "https://url2.com/path?key=value"));
  auto expected_match_3 =
      AllOf(Field(&AutocompleteMatch::relevance, 250),
            Field(&AutocompleteMatch::type,
                  AutocompleteMatchType::HISTORY_EMBEDDINGS),
            Field(&AutocompleteMatch::destination_url, "https://url3.com/"));

  // Expect only non-answer matches.
  EXPECT_FALSE(history_embeddings_provider_->done_);
  EXPECT_THAT(
      history_embeddings_provider_->matches_,
      ElementsAre(expected_match_1, expected_match_2, expected_match_3));

  // 2nd response with status `kLoading`.
  auto result_loading = result.Clone();
  result_loading.answerer_result.status = kLoading;
  result_loading.answerer_result.query = "query";
  history_embeddings_provider_->OnReceivedSearchResult(
      std::move(result_loading));

  // Expect the same non-answer matches as well as a 'loading' answer match,
  // scored 1 less than the top match.
  EXPECT_FALSE(history_embeddings_provider_->done_);
  EXPECT_THAT(
      history_embeddings_provider_->matches_,
      ElementsAre(
          expected_match_1, expected_match_2, expected_match_3,
          AllOf(Field(&AutocompleteMatch::relevance, 749),
                Field(&AutocompleteMatch::type,
                      AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER),
                Field(&AutocompleteMatch::destination_url, ""),
                Field(&AutocompleteMatch::history_embeddings_answer_header_text,
                      u"Searching for a summary\x2026"),
                Field(&AutocompleteMatch::description, u""))));

  // 3rd response with status `kSuccess`.
  auto result_success = result.Clone();
  result_success.answerer_result.status = kSuccess;
  result_success.answerer_result.query = "query";
  result_success.answerer_result.answer.set_score(1);
  result_success.answerer_result.answer.set_text("answer text");
  result_success.answerer_result.url = "https://url2.com/path?key=value";
  history_embeddings_provider_->OnReceivedSearchResult(
      std::move(result_success));

  // Expect the same non-answer matches as well as a flushed out answer match,
  // scored 1 less than the corresponding match.
  EXPECT_TRUE(history_embeddings_provider_->done_);
  EXPECT_THAT(
      history_embeddings_provider_->matches_,
      ElementsAre(
          expected_match_1, expected_match_2, expected_match_3,
          AllOf(Field(&AutocompleteMatch::relevance, 499),
                Field(&AutocompleteMatch::type,
                      AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER),
                Field(&AutocompleteMatch::destination_url,
                      "chrome://history/?q=query"),
                Field(&AutocompleteMatch::history_embeddings_answer_header_text,
                      u"Summary"),
                Field(&AutocompleteMatch::description, u"answer text"),
                Field(&AutocompleteMatch::contents,
                      u"url2.com  •  Visited Mar 23, 2025"))));

  // Test error cases.
  std::u16string execution_error =
      u"Something went wrong. Please try again later.";
  std::u16string non_temporary_error = u"Sorry, I can't help you with that.";
  for (const auto& [status, expected_answer_text] : std::vector<
           std::pair<history_embeddings::ComputeAnswerStatus, std::u16string>>{
           {kUnanswerable, u""},
           {kModelUnavailable, u""},
           {kExecutionFailure, execution_error},
           {kExecutionCancelled, u""},
           {kFiltered, u""}}) {
    SCOPED_TRACE("Testing status: " +
                 base::NumberToString(static_cast<int>(status)));
    auto result_error = result.Clone();
    result_error.answerer_result.status = status;
    history_embeddings_provider_->done_ = false;
    history_embeddings_provider_->OnReceivedSearchResult(
        std::move(result_error));

    EXPECT_TRUE(history_embeddings_provider_->done_);
    if (expected_answer_text.empty()) {
      EXPECT_THAT(
          history_embeddings_provider_->matches_,
          ElementsAre(expected_match_1, expected_match_2, expected_match_3));
    } else {
      EXPECT_THAT(
          history_embeddings_provider_->matches_,
          ElementsAre(
              expected_match_1, expected_match_2, expected_match_3,
              AllOf(
                  Field(&AutocompleteMatch::relevance, 749),
                  Field(&AutocompleteMatch::type,
                        AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER),
                  Field(&AutocompleteMatch::destination_url, ""),
                  Field(
                      &AutocompleteMatch::history_embeddings_answer_header_text,
                      u"Summary"),
                  Field(&AutocompleteMatch::description, expected_answer_text),
                  Field(&AutocompleteMatch::contents, u""))));
    }
  }
}
