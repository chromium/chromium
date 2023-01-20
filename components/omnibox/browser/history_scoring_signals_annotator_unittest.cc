// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_scoring_signals_annotator.h"

#include "base/test/task_environment.h"
#include "components/bookmarks/test/test_bookmark_client.h"
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
  client_->set_template_url_service(
      std::make_unique<TemplateURLService>(nullptr, 0));
  annotator_ = std::make_unique<HistoryScoringSignalsAnnotator>(client_.get());
  FillHistoryDbData();
  CreateAutocompleteResult();
}

void HistoryScoringSignalsAnnotatorTest::FillHistoryDbData() {
  const base::Time now = base::Time::Now();
  history::URLRow row_1 = CreateUrlRow("http://test.com/", u"A Title", 2, 5,
                                       now - base::Days(1), 1);
  history::URLRow row_2 = CreateUrlRow(
      "http://test.com/path", u"A Title - Path", 1, 1, now - base::Days(2), 2);
  client_->GetHistoryService()->InMemoryDatabase()->AddURL(row_1);
  client_->GetHistoryService()->InMemoryDatabase()->AddURL(row_2);
}

void HistoryScoringSignalsAnnotatorTest::CreateAutocompleteResult() {
  AutocompleteMatch url_match;
  url_match.destination_url = GURL("http://test.com/");
  url_match.type = AutocompleteMatchType::HISTORY_URL;

  // Search matches will be skipped for annotation.
  AutocompleteMatch search_match;
  search_match.type = AutocompleteMatchType::SEARCH_HISTORY;

  std::vector<AutocompleteMatch> matches{url_match, search_match};
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
  EXPECT_EQ(result()->match_at(0)->scoring_signals.typed_count(), 2);
  EXPECT_EQ(result()->match_at(0)->scoring_signals.visit_count(), 5);
  EXPECT_TRUE(
      result()->match_at(0)->scoring_signals.elapsed_time_last_visit_secs() >
      0);
  EXPECT_EQ(result()->match_at(0)->scoring_signals.total_title_match_length(),
            3);
  EXPECT_EQ(
      result()->match_at(0)->scoring_signals.num_input_terms_matched_by_title(),
      2);
  EXPECT_FALSE(result()->match_at(1)->scoring_signals.has_typed_count());
  EXPECT_FALSE(result()
                   ->match_at(1)
                   ->scoring_signals.has_num_input_terms_matched_by_title());
}
