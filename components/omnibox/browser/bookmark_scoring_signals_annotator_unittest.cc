// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/bookmark_scoring_signals_annotator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace {

// The bookmark test data.
struct BookmarksTestInfo {
  std::string title;
  std::string url;
};

std::vector<BookmarksTestInfo> bookmark_test_data = {
    {"defgh abc", "http://testa.com"},
    {"abc defgh", "http://testa.com"},
    {"edef abc", "http://testb.com"},
    {"aaa", "http://testa.com/a"},
};

}  // namespace

class BookmarkScoringSignalsAnnotatorTest : public testing::Test {
 public:
  BookmarkScoringSignalsAnnotatorTest() = default;
  BookmarkScoringSignalsAnnotatorTest(
      const BookmarkScoringSignalsAnnotatorTest&) = delete;
  BookmarkScoringSignalsAnnotatorTest& operator=(
      const BookmarkScoringSignalsAnnotatorTest&) = delete;

  AutocompleteProviderClient* client() { return client_.get(); }
  AutocompleteScoringSignalsAnnotator* annotator() { return annotator_.get(); }
  AutocompleteResult* result() { return result_.get(); }

  void SetUp() override;
  void TearDown() override;

 private:
  void FillBookmarkModelData();
  void CreateAutocompleteResult();

  base::ScopedTempDir history_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  std::unique_ptr<BookmarkScoringSignalsAnnotator> annotator_;
  std::unique_ptr<AutocompleteResult> result_;
};

void BookmarkScoringSignalsAnnotatorTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();
  CHECK(history_dir_.CreateUniqueTempDir());
  client_->set_history_service(history::CreateHistoryService(
      history_dir_.GetPath(), /*create_db=*/true));
  client_->set_bookmark_model(bookmarks::TestBookmarkClient::CreateModel());
  annotator_ = std::make_unique<BookmarkScoringSignalsAnnotator>(client_.get());
  FillBookmarkModelData();
  CreateAutocompleteResult();
}

void BookmarkScoringSignalsAnnotatorTest::TearDown() {
  task_environment_.RunUntilIdle();
}

void BookmarkScoringSignalsAnnotatorTest::FillBookmarkModelData() {
  auto* model = client_->GetBookmarkModel();
  const bookmarks::BookmarkNode* other_node = model->other_node();
  for (const auto& test_data : bookmark_test_data) {
    const BookmarksTestInfo& cur(test_data);
    const GURL url(cur.url);
    model->AddURL(other_node, other_node->children().size(),
                  base::ASCIIToUTF16(cur.title), url);
  }
}

void BookmarkScoringSignalsAnnotatorTest::CreateAutocompleteResult() {
  AutocompleteMatch url_match_c;
  url_match_c.destination_url = GURL("http://testc.com/");
  url_match_c.type = AutocompleteMatchType::HISTORY_URL;

  AutocompleteMatch url_match_b;
  url_match_b.destination_url = GURL("http://testb.com/");
  url_match_b.type = AutocompleteMatchType::HISTORY_URL;

  AutocompleteMatch url_match_a;
  url_match_a.destination_url = GURL("http://testa.com/");
  url_match_a.type = AutocompleteMatchType::HISTORY_URL;

  std::vector<AutocompleteMatch> matches{url_match_c, url_match_b, url_match_a};
  result_ = std::make_unique<AutocompleteResult>();
  result_->AppendMatches(matches);
}

TEST_F(BookmarkScoringSignalsAnnotatorTest, AnnotateResult) {
  AutocompleteInput input(u"abc", std::u16string::npos,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  // url_match_c
  annotator()->AnnotateResult(input, result());
  EXPECT_EQ(result()->match_at(0)->scoring_signals->num_bookmarks_of_url(), 0);
  EXPECT_EQ(result()
                ->match_at(0)
                ->scoring_signals->total_bookmark_title_match_length(),
            0);
  EXPECT_EQ(result()
                ->match_at(0)
                ->scoring_signals->first_bookmark_title_match_position(),
            0);

  // url_match_b
  annotator()->AnnotateResult(input, result());
  EXPECT_EQ(result()->match_at(1)->scoring_signals->num_bookmarks_of_url(), 1);
  EXPECT_EQ(result()
                ->match_at(1)
                ->scoring_signals->total_bookmark_title_match_length(),
            3);
  EXPECT_EQ(result()
                ->match_at(1)
                ->scoring_signals->first_bookmark_title_match_position(),
            5);

  // url_match_a
  annotator()->AnnotateResult(input, result());
  EXPECT_EQ(result()->match_at(2)->scoring_signals->num_bookmarks_of_url(), 2);
  EXPECT_EQ(result()
                ->match_at(2)
                ->scoring_signals->total_bookmark_title_match_length(),
            3);
  EXPECT_EQ(result()
                ->match_at(2)
                ->scoring_signals->first_bookmark_title_match_position(),
            0);
}
