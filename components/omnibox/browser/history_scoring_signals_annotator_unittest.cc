// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_scoring_signals_annotator.h"

#include "base/test/task_environment.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"

using ::history::URLRow;

namespace {

const history::KeywordID kTestKeywordId = 42;

URLRow CreateUrlRow(const std::string& url,
                    const std::u16string& title,
                    int typed_count,
                    int visit_count,
                    const base::Time& last_visit,
                    int64_t id) {
  GURL gurl(url);
  URLRow row(gurl);
  row.set_title(title);
  row.set_typed_count(typed_count);
  row.set_visit_count(visit_count);
  row.set_last_visit(last_visit);
  row.set_hidden(false);
  row.set_id(id);
  return row;
}

}  // namespace

class HistoryScoringSignalsAnnotatorTest : public testing::Test {
 public:
  HistoryScoringSignalsAnnotatorTest() = default;

  AutocompleteProviderClient* client() { return client_.get(); }
  AutocompleteScoringSignalsAnnotator* annotator() { return annotator_.get(); }
  AutocompleteResult* result() { return result_.get(); }

  void SetUp() override;
  void TearDown() override;

 private:
  void FillHistoryDbData();
  void CreateAutocompleteResult();

  base::ScopedTempDir history_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  std::unique_ptr<HistoryScoringSignalsAnnotator> annotator_;
  std::unique_ptr<AutocompleteResult> result_;
};

void HistoryScoringSignalsAnnotatorTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();
  CHECK(history_dir_.CreateUniqueTempDir());
  client_->set_history_service(history::CreateHistoryService(
      history_dir_.GetPath(), /*create_db=*/true));
  client_->set_bookmark_model(bookmarks::TestBookmarkClient::CreateModel());
  annotator_ = std::make_unique<HistoryScoringSignalsAnnotator>(client_.get());
  FillHistoryDbData();
  CreateAutocompleteResult();
}

void HistoryScoringSignalsAnnotatorTest::FillHistoryDbData() {
  const base::Time now = base::Time::Now();
  // Add some regular web URL visits to the DB.
  history::URLRow row_1 = CreateUrlRow("http://test.com/", u"A Title", 2, 5,
                                       now - base::Days(1), 1);
  history::URLRow row_2 = CreateUrlRow(
      "http://test.com/path", u"A Title - Path", 1, 1, now - base::Days(2), 2);
  client_->GetHistoryService()->InMemoryDatabase()->AddURL(row_1);
  client_->GetHistoryService()->InMemoryDatabase()->AddURL(row_2);

  // Add some SRP URL visits to the DB.
  history::URLRow row_3 =
      CreateUrlRow("https://google.com/search?q=hello&p=c3d4",
                   u"hello - Google Search", 0, 4, now - base::Days(4), 3);
  history::URLRow row_4 =
      CreateUrlRow("https://google.com/search?q=hello&p=e5f6",
                   u"hello - Google Search", 0, 2, now - base::Days(2), 4);
  client_->GetHistoryService()->InMemoryDatabase()->AddURL(row_3);
  client_->GetHistoryService()->InMemoryDatabase()->SetKeywordSearchTermsForURL(
      row_3.id(), kTestKeywordId, u"hello");
  client_->GetHistoryService()->InMemoryDatabase()->AddURL(row_4);
  client_->GetHistoryService()->InMemoryDatabase()->SetKeywordSearchTermsForURL(
      row_4.id(), kTestKeywordId, u"hello");
}

void HistoryScoringSignalsAnnotatorTest::CreateAutocompleteResult() {
  AutocompleteMatch url_match_not_in_db;
  url_match_not_in_db.destination_url = GURL("http://test1.com/");
  url_match_not_in_db.type = AutocompleteMatchType::HISTORY_URL;

  AutocompleteMatch url_match;
  url_match.destination_url = GURL("http://test.com/");
  url_match.type = AutocompleteMatchType::HISTORY_URL;

  // Search matches will be skipped for annotation.
  AutocompleteMatch search_match;
  search_match.contents = u"hello";
  search_match.destination_url =
      GURL("https://google.com/search?q=hello&p=a1b2");
  search_match.type = AutocompleteMatchType::SEARCH_HISTORY;

  std::vector<AutocompleteMatch> matches{url_match_not_in_db, url_match,
                                         search_match};
  result_ = std::make_unique<AutocompleteResult>();
  result_->AppendMatches(matches);
}

void HistoryScoringSignalsAnnotatorTest::TearDown() {
  task_environment_.RunUntilIdle();
}

TEST_F(HistoryScoringSignalsAnnotatorTest, AnnotateResult) {
  AutocompleteInput input(u"a ti xyz", std::u16string::npos,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  annotator()->AnnotateResult(input, result());

  // First match is not in the History DB, signals cannot be annotated.
  EXPECT_FALSE(result()->match_at(0)->scoring_signals->has_typed_count());

  // Match is a history URL and available in the history DB, ensure signals are
  // annotated.
  EXPECT_TRUE(result()->match_at(1)->scoring_signals.has_value());
  EXPECT_TRUE(result()->match_at(1)->scoring_signals->has_typed_count());
  EXPECT_EQ(result()->match_at(1)->scoring_signals->typed_count(), 2);
  EXPECT_EQ(result()->match_at(1)->scoring_signals->visit_count(), 5);
  EXPECT_TRUE(
      result()->match_at(1)->scoring_signals->elapsed_time_last_visit_secs() >
      0);
  EXPECT_EQ(result()->match_at(1)->scoring_signals->total_title_match_length(),
            3);
  EXPECT_EQ(result()
                ->match_at(1)
                ->scoring_signals->num_input_terms_matched_by_title(),
            2);

  // Search results are skipped for annotation.
  EXPECT_FALSE(result()->match_at(2)->scoring_signals.has_value());
}
