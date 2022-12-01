// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/bookmark_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::TitledUrlMatch;

namespace {

// The bookmark corpus against which we will simulate searches.
struct BookmarksTestInfo {
  std::string title;
  std::string url;
} bookmark_provider_test_data[] = {
    {"abc def", "http://www.catsanddogs.com/a"},
    {"abcde", "http://www.catsanddogs.com/b"},
    {"abcdef", "http://www.catsanddogs.com/c"},
    {"carry carbon carefully", "http://www.catsanddogs.com/d"},
    {"a definition", "http://www.catsanddogs.com/e"},
    {"ghi jkl", "http://www.catsanddogs.com/f"},
    {"jkl ghi", "http://www.catsanddogs.com/g"},
    {"frankly frankly frank", "http://www.catsanddogs.com/h"},
    {"foobar foobar", "http://www.foobar.com/"},
    {"domain", "http://www.domain.com/http/"},
    {"repeat", "http://www.repeat.com/1/repeat/2/"},
    // For testing inline_autocompletion.
    {"http://blah.com/", "http://blah.com/"},
    {"http://fiddle.com/", "http://fiddle.com/"},
    {"http://www.www.com/", "http://www.www.com/"},
    {"chrome://version", "chrome://version"},
    {"chrome://omnibox", "chrome://omnibox"},
    // For testing ranking with different URLs.
    {"achlorhydric featherheads resuscitates mockingbirds",
     "http://www.manylongwords.com/1a"},
    {"achlorhydric mockingbirds resuscitates featherhead",
     "http://www.manylongwords.com/2b"},
    {"featherhead resuscitates achlorhydric mockingbirds",
     "http://www.manylongwords.com/3c"},
    {"mockingbirds resuscitates featherheads achlorhydric",
     "http://www.manylongwords.com/4d"},
    // For testing URL boosting.  (URLs referenced multiple times are boosted.)
    {"burning worms #1", "http://www.burns.com/"},
    {"burning worms #2", "http://www.worms.com/"},
    {"worming burns #10", "http://www.burns.com/"},
    // For testing strange spacing in bookmark titles.
    {" hello1  hello2  ", "http://whatever.com/"},
    {"", "http://emptytitle.com/"},
    // For testing short bookmarks.
    {"testing short bookmarks", "https://zzz.com"},
    // For testing bookmarks search in keyword mode.
    {"@bookmarks", "chrome://bookmarks"},
    // For testing max matches.
    {"zyx1", "http://randomsite.com/zyx1"},
    {"zyx2", "http://randomsite.com/zyx2"},
    {"zyx3", "http://randomsite.com/zyx3"},
    {"zyx4", "http://randomsite.com/zyx4"},
    {"zyx5", "http://randomsite.com/zyx5"},
    {"zyx6", "http://randomsite.com/zyx6"},
    {"zyx7", "http://randomsite.com/zyx7"},
    {"zyx8", "http://randomsite.com/zyx8"},
    {"zyx9", "http://randomsite.com/zyx9"},
};

// Structures and functions supporting the BookmarkProviderTest.Positions
// unit test.

struct TestBookmarkPosition {
  TestBookmarkPosition(size_t begin, size_t end)
      : begin(begin), end(end) {}

  size_t begin;
  size_t end;
};
typedef std::vector<TestBookmarkPosition> TestBookmarkPositions;

// Return |positions| as a formatted string for unit test diagnostic output.
std::string TestBookmarkPositionsAsString(
    const TestBookmarkPositions& positions) {
  std::string position_string("{");
  for (auto i = positions.begin(); i != positions.end(); ++i) {
    if (i != positions.begin())
      position_string += ", ";
    position_string += "{" + base::NumberToString(i->begin) + ", " +
                       base::NumberToString(i->end) + "}";
  }
  position_string += "}\n";
  return position_string;
}

// Return the positions in |matches| as a formatted string for unit test
// diagnostic output.
std::u16string MatchesAsString16(const ACMatches& matches) {
  std::u16string matches_string;
  for (auto i = matches.begin(); i != matches.end(); ++i) {
    matches_string.append(u"    '");
    matches_string.append(i->description);
    matches_string.append(u"'\n");
  }
  return matches_string;
}

// Comparison function for sorting search terms by descending length.
bool TestBookmarkPositionsEqual(const TestBookmarkPosition& pos_a,
                                const TestBookmarkPosition& pos_b) {
  return pos_a.begin == pos_b.begin && pos_a.end == pos_b.end;
}

// Convience function to make comparing ACMatchClassifications against the
// test expectations structure easier.
TestBookmarkPositions PositionsFromAutocompleteMatch(
    const AutocompleteMatch& match) {
  TestBookmarkPositions positions;
  bool started = false;
  size_t start = 0;
  for (auto i = match.description_class.begin();
       i != match.description_class.end(); ++i) {
    if (i->style & AutocompleteMatch::ACMatchClassification::MATCH) {
      // We have found the start of a match.
      EXPECT_FALSE(started);
      started = true;
      start = i->offset;
    } else if (started) {
      // We have found the end of a match.
      started = false;
      positions.push_back(TestBookmarkPosition(start, i->offset));
      start = 0;
    }
  }
  // Record the final position if the last match goes to the end of the
  // candidate string.
  if (started)
    positions.push_back(TestBookmarkPosition(start, match.description.size()));
  return positions;
}

// Convenience function to make comparing test expectations structure against
// the actual ACMatchClassifications easier.
TestBookmarkPositions PositionsFromExpectations(
    const size_t expectations[9][2]) {
  TestBookmarkPositions positions;
  size_t i = 0;
  // The array is zero-terminated in the [1]th element.
  while (expectations[i][1]) {
    positions.push_back(
        TestBookmarkPosition(expectations[i][0], expectations[i][1]));
    ++i;
  }
  return positions;
}

}  // namespace

class BookmarkProviderTest : public testing::Test {
 public:
  BookmarkProviderTest();
  BookmarkProviderTest(const BookmarkProviderTest&) = delete;
  BookmarkProviderTest& operator=(const BookmarkProviderTest&) = delete;

 protected:
  void SetUp() override;

  // Invokes |Start()| with |input_text| and verifies the number of matches
  // returned and whether |expected_triggered_feature| was triggered. If
  // |expected_triggered_feature| is empty, verifies no feature was triggered.
  void TestNumMatchesAndTriggeredFeature(
      std::string input_text,
      size_t expected_matches_count,
      absl::optional<OmniboxTriggeredFeatureService::Feature>
          expected_triggered_feature = {});

  std::unique_ptr<MockAutocompleteProviderClient> provider_client_;
  std::unique_ptr<BookmarkModel> model_;
  scoped_refptr<BookmarkProvider> provider_;
  TestSchemeClassifier classifier_;
};

BookmarkProviderTest::BookmarkProviderTest() {
  model_ = bookmarks::TestBookmarkClient::CreateModel();
}

void BookmarkProviderTest::SetUp() {
  provider_client_ = std::make_unique<MockAutocompleteProviderClient>();
  EXPECT_CALL(*provider_client_, GetBookmarkModel())
      .WillRepeatedly(testing::Return(model_.get()));
  EXPECT_CALL(*provider_client_, GetSchemeClassifier())
      .WillRepeatedly(testing::ReturnRef(classifier_));

  provider_client_->set_template_url_service(
      std::make_unique<TemplateURLService>(nullptr, 0));

  provider_ = new BookmarkProvider(provider_client_.get());
  const BookmarkNode* other_node = model_->other_node();
  for (size_t i = 0; i < std::size(bookmark_provider_test_data); ++i) {
    const BookmarksTestInfo& cur(bookmark_provider_test_data[i]);
    const GURL url(cur.url);
    model_->AddURL(other_node, other_node->children().size(),
                   base::ASCIIToUTF16(cur.title), url);
  }
}

void BookmarkProviderTest::TestNumMatchesAndTriggeredFeature(
    std::string input_text,
    size_t expected_matches_count,
    absl::optional<OmniboxTriggeredFeatureService::Feature>
        expected_triggered_feature) {
  SCOPED_TRACE("[" + input_text + "]");  // Wrap |input_text| in `[]` to make
                                         // trailing whitespace apparent.

  AutocompleteInput input(base::UTF8ToUTF16(input_text),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->Start(input, false);
  EXPECT_EQ(provider_->matches().size(), expected_matches_count);

  auto* triggered_feature_service =
      provider_client_->GetOmniboxTriggeredFeatureService();
  OmniboxTriggeredFeatureService::Features triggered_features;
  triggered_feature_service->RecordToLogs(&triggered_features);
  triggered_feature_service->ResetSession();
  if (expected_triggered_feature) {
    ASSERT_TRUE(!triggered_features.empty());
    EXPECT_EQ(*triggered_features.begin(), *expected_triggered_feature);
  } else
    EXPECT_TRUE(triggered_features.empty());
}

TEST_F(BookmarkProviderTest, Positions) {
  // Simulate searches.
  // Description of |positions|:
  //   The first index represents the collection of positions for each expected
  //   match. The count of the actual subarrays in each instance of |query_data|
  //   must equal |match_count|. The second index represents each expected
  //   match position. The third index represents the |start| and |end| of the
  //   expected match's position within the |test_data|. This array must be
  //   terminated by an entry with a value of '0' for |end|.
  // Example:
  //   Consider the line for 'def' below:
  //     {"def", 2, {{{4, 7}, {XXX, 0}}, {{2, 5}, {11, 14}, {XXX, 0}}}},
  //   There are two expected matches:
  //     0. {{4, 7}, {XXX, 0}}
  //     1. {{2, 5}, {11 ,14}, {XXX, 0}}
  //   For the first match, [0], there is one match within the bookmark's title
  //   expected, {4, 7}, which maps to the 'def' within "abc def". The 'XXX'
  //   value is ignored. The second match, [1], indicates that two matches are
  //   expected within the bookmark title "a definite definition". In each case,
  //   the {XXX, 0} indicates the end of the subarray. Or:
  //                 Match #1            Match #2
  //                 ------------------  ----------------------------
  //                  Pos1    Term        Pos1    Pos2      Term
  //                  ------  --------    ------  --------  --------
  //     {"def", 2, {{{4, 7}, {999, 0}}, {{2, 5}, {11, 14}, {999, 0}}}},
  //
  struct QueryData {
    const std::string query;
    const size_t match_count;  // This count must match the number of major
                               // elements in the following |positions| array.
    const size_t positions[99][9][2];
  } query_data[] = {
      // This first set is primarily for position detection validation.
      {"abc", 3, {{{0, 3}, {0, 0}}, {{0, 3}, {0, 0}}, {{0, 3}, {0, 0}}}},
      {"abcde", 2, {{{0, 5}, {0, 0}}, {{0, 5}, {0, 0}}}},
      {"foo bar", 0, {{{0, 0}}}},
      {"fooey bark", 0, {{{0, 0}}}},
      {"def", 2, {{{2, 5}, {0, 0}}, {{4, 7}, {0, 0}}}},
      {"ghi jkl", 2, {{{0, 7}, {0, 0}}, {{0, 3}, {4, 7}, {0, 0}}}},
      // NB: GetBookmarksMatching(...) uses exact match for "a" in title or URL.
      {"a", 2, {{{0, 1}, {0, 0}}, {{0, 1}, {0, 0}}}},
      {"a d", 0, {{{0, 0}}}},
      {"carry carbon", 1, {{{0, 12}, {0, 0}}}},
      // NB: GetBookmarksMatching(...) sorts the match positions.
      {"carbon carry", 1, {{{0, 5}, {6, 12}, {0, 0}}}},
      {"arbon", 0, {{{0, 0}}}},
      {"ar", 0, {{{0, 0}}}},
      {"arry", 0, {{{0, 0}}}},
      // Quoted terms are single terms.
      {"\"carry carbon\"", 1, {{{0, 5}, {6, 12}, {0, 0}}}},
      {"\"carry carbon\" care", 1, {{{0, 5}, {6, 12}, {13, 17}, {0, 0}}}},
      // Quoted terms require complete word matches.
      {"\"carry carbo\"", 0, {{{0, 0}}}},
      // This set uses duplicated and/or overlaps search terms in the title.
      {"frank", 1, {{{0, 5}, {0, 0}}}},
      {"frankly", 1, {{{0, 7}, {0, 0}}}},
      {"frankly frankly", 1, {{{0, 15}, {0, 0}}}},
      {"foobar foo", 1, {{{0, 10}, {0, 0}}}},
      {"foo foobar", 1, {{{0, 6}, {7, 13}, {0, 0}}}},
      // This ensures that leading whitespace in the title is correctly offset.
      {"hello", 1, {{{0, 5}, {0, 0}}}},
      // This ensures that empty titles yield empty classifications.
      {"emptytitle", 1, {}},
  };

  for (size_t i = 0; i < std::size(query_data); ++i) {
    AutocompleteInput input(base::ASCIIToUTF16(query_data[i].query),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const ACMatches& matches(provider_->matches());
    // Validate number of results is as expected.
    EXPECT_LE(matches.size(), query_data[i].match_count)
        << "One or more of the following matches were unexpected:\n"
        << MatchesAsString16(matches) << "For query '" << query_data[i].query
        << "'.";
    EXPECT_GE(matches.size(), query_data[i].match_count)
        << "One or more expected matches are missing. Matches found:\n"
        << MatchesAsString16(matches) << "for query '" << query_data[i].query
        << "'.";
    // Validate positions within each match is as expected.
    for (size_t j = 0; j < matches.size(); ++j) {
      // Collect the expected positions as a vector, collect the match's
      // classifications for match positions as a vector, then compare.
      TestBookmarkPositions expected_positions(
          PositionsFromExpectations(query_data[i].positions[j]));
      TestBookmarkPositions actual_positions(
          PositionsFromAutocompleteMatch(matches[j]));
      EXPECT_TRUE(base::ranges::equal(expected_positions, actual_positions,
                                      TestBookmarkPositionsEqual))
          << "EXPECTED: " << TestBookmarkPositionsAsString(expected_positions)
          << "ACTUAL:   " << TestBookmarkPositionsAsString(actual_positions)
          << "    for query: '" << query_data[i].query << "'.";
    }
  }
}

TEST_F(BookmarkProviderTest, Rankings) {
  // Simulate searches.
  struct QueryData {
    const std::string query;
    // |match_count| must match the number of elements in the following
    // |matches| array.
    const size_t match_count;
    // |matches| specifies the titles for all bookmarks expected to be matched
    // by the |query|
    const std::string matches[3];
  } query_data[] = {
    // Basic ranking test.
    {"abc",       3, {"abcde",      // Most complete match.
                      "abcdef",
                      "abc def"}},  // Least complete match.
    {"ghi",       2, {"ghi jkl",    // Matched earlier.
                      "jkl ghi",    // Matched later.
                      ""}},
    // Rankings of exact-word matches with different URLs.
    {"achlorhydric",
                  3, {"achlorhydric mockingbirds resuscitates featherhead",
                      "achlorhydric featherheads resuscitates mockingbirds",
                      "featherhead resuscitates achlorhydric mockingbirds"}},
    {"achlorhydric featherheads",
                  2, {"achlorhydric featherheads resuscitates mockingbirds",
                      "mockingbirds resuscitates featherheads achlorhydric",
                      ""}},
    {"mockingbirds resuscitates",
                  3, {"mockingbirds resuscitates featherheads achlorhydric",
                      "achlorhydric mockingbirds resuscitates featherhead",
                      "featherhead resuscitates achlorhydric mockingbirds"}},
    // Ranking of exact-word matches with URL boosts.
    {"worms",     2, {"burning worms #1",    // boosted
                      "burning worms #2",    // not boosted
                      ""}},
    // Ranking of prefix matches with URL boost.
    {"burn worm", 3, {"burning worms #1",    // boosted
                      "worming burns #10",   // boosted but longer title
                      "burning worms #2"}},  // not boosted
    // A query of "worm burn" will have the same results.
    {"worm burn", 3, {"burning worms #1",    // boosted
                      "worming burns #10",   // boosted but longer title
                      "burning worms #2"}},  // not boosted
  };

  for (size_t i = 0; i < std::size(query_data); ++i) {
    AutocompleteInput input(base::ASCIIToUTF16(query_data[i].query),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const ACMatches& matches(provider_->matches());
    // Validate number and content of results is as expected.
    for (size_t j = 0; j < std::max(query_data[i].match_count, matches.size());
         ++j) {
      EXPECT_LT(j, query_data[i].match_count)
          << "    Unexpected match '"
          << base::UTF16ToUTF8(matches[j].description) << "' for query: '"
          << query_data[i].query << "'.";
      if (j >= query_data[i].match_count)
        continue;
      EXPECT_LT(j, matches.size())
          << "    Missing match '" << query_data[i].matches[j]
          << "' for query: '" << query_data[i].query << "'.";
      if (j >= matches.size())
        continue;
      EXPECT_EQ(query_data[i].matches[j],
                base::UTF16ToUTF8(matches[j].description))
          << "    Mismatch at [" << base::NumberToString(j) << "] for query '"
          << query_data[i].query << "'.";
    }
  }
}

TEST_F(BookmarkProviderTest, InlineAutocompletion) {
  // Simulate searches.
  struct QueryData {
    const std::string query;
    const std::string url;
    const bool allowed_to_be_default_match;
    const std::string inline_autocompletion;
  } query_data[] = {
      {"bla", "http://blah.com/", true, "h.com"},
      {"blah ", "http://blah.com/", false, ".com"},
      {"http://bl", "http://blah.com/", true, "ah.com"},
      {"fiddle.c", "http://fiddle.com/", true, "om"},
      {"www", "http://www.www.com/", true, ".com"},
      {"chro", "chrome://version", true, "me://version"},
      {"chrome://ve", "chrome://version", true, "rsion"},
      {"chrome ver", "chrome://version", false, ""},
      {"versi", "chrome://version", false, ""},
      {"abou", "chrome://omnibox", false, ""},
      {"about:om", "chrome://omnibox", true, "nibox"}
      // Note: when adding a new URL to this test, be sure to add it to the list
      // of bookmarks at the top of the file as well.  All items in this list
      // need to be in the bookmarks list because BookmarkProvider's
      // TitleMatchToACMatch() has an assertion that verifies the URL is
      // actually bookmarked.
  };

  for (size_t i = 0; i < std::size(query_data); ++i) {
    const std::string description =
        "for query=" + query_data[i].query + " and url=" + query_data[i].url;
    AutocompleteInput input(base::ASCIIToUTF16(query_data[i].query),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    const std::u16string fixed_up_input(
        provider_->FixupUserInput(input).second);
    BookmarkNode node(/*id=*/0, base::GUID::GenerateRandomV4(),
                      GURL(query_data[i].url));
    node.SetTitle(base::ASCIIToUTF16(query_data[i].url));
    TitledUrlMatch bookmark_match;
    bookmark_match.node = &node;
    auto relevance_and_bookmark_count =
        provider_->CalculateBookmarkMatchRelevance(bookmark_match);
    const AutocompleteMatch& ac_match = TitledUrlMatchToAutocompleteMatch(
        bookmark_match, AutocompleteMatchType::BOOKMARK_TITLE,
        relevance_and_bookmark_count.first, relevance_and_bookmark_count.second,
        provider_.get(), classifier_, input, fixed_up_input);
    EXPECT_EQ(query_data[i].allowed_to_be_default_match,
              ac_match.allowed_to_be_default_match)
        << description;
    EXPECT_EQ(base::ASCIIToUTF16(query_data[i].inline_autocompletion),
              ac_match.inline_autocompletion)
        << description;
  }
}

TEST_F(BookmarkProviderTest, StripHttpAndAdjustOffsets) {
  // Simulate searches.
  struct QueryData {
    const std::string query;
    const std::string expected_contents;
    // |expected_contents_class| is in format offset:style,offset:style,...
    const std::string expected_contents_class;
  } query_data[] = {
      // clang-format off
    { "foo",       "foobar.com",              "0:3,3:1"                    },
    { "www foo",   "www.foobar.com",          "0:3,3:1,4:3,7:1"            },
    { "foo www",   "www.foobar.com",          "0:3,3:1,4:3,7:1"            },
    { "foo http",  "http://foobar.com",       "0:3,4:1,7:3,10:1"           },
    { "blah",      "blah.com",                "0:3,4:1"                    },
    { "http blah", "http://blah.com",         "0:3,4:1,7:3,11:1"           },
    { "dom",       "domain.com/http/",        "0:3,3:1"                    },
    { "dom http",  "http://domain.com/http/", "0:3,4:1,7:3,10:1,18:3,22:1" },
    { "rep",       "repeat.com/1/repeat/2/",  "0:3,3:1"                    },
    { "versi",     "chrome://version",        "0:1,9:3,14:1"               },
      // clang-format on
  };

  for (size_t i = 0; i < std::size(query_data); ++i) {
    std::string description = "for query=" + query_data[i].query;
    AutocompleteInput input(base::ASCIIToUTF16(query_data[i].query),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const ACMatches& matches(provider_->matches());
    ASSERT_EQ(1U, matches.size()) << description;
    const AutocompleteMatch& match = matches[0];

    description +=
        "\n EXPECTED classes: " + query_data[i].expected_contents_class;
    description += "\n ACTUAL classes:  ";
    for (auto x : match.contents_class) {
      description += base::NumberToString(x.offset) + ":" +
                     base::NumberToString(x.style) + ",";
    }

    EXPECT_EQ(base::ASCIIToUTF16(query_data[i].expected_contents),
              match.contents) << description;
    std::vector<std::string> class_strings = base::SplitString(
        query_data[i].expected_contents_class, ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(class_strings.size(), match.contents_class.size())
        << description;
    for (size_t j = 0; j < class_strings.size(); ++j) {
      std::vector<std::string> chunks = base::SplitString(
          class_strings[j], ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      ASSERT_EQ(2U, chunks.size()) << description;
      size_t offset;
      EXPECT_TRUE(base::StringToSizeT(chunks[0], &offset)) << description;
      EXPECT_EQ(offset, match.contents_class[j].offset) << description;
      int style;
      EXPECT_TRUE(base::StringToInt(chunks[1], &style)) << description;
      EXPECT_EQ(style, match.contents_class[j].style) << description;
    }
  }
}

TEST_F(BookmarkProviderTest, DoesNotProvideMatchesOnFocus) {
  AutocompleteInput input(u"foo", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(BookmarkProviderTest, ShortBookmarks) {
  // Test the 2 short bookmark features that determine when short inputs should
  // be allowed to prefix match. These tests are trying to match the mock
  // bookmark "testing short bookmarks".

  auto trigger_feature = OmniboxTriggeredFeatureService::Feature::
      kShortBookmarkSuggestionsByTotalInputLength;

  {
    SCOPED_TRACE("Default.");
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 0);
  }

  {
    SCOPED_TRACE("Short bookmarks enabled.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kShortBookmarkSuggestions);
    TestNumMatchesAndTriggeredFeature("te", 1);
    TestNumMatchesAndTriggeredFeature("te ", 1);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 1);
  }

  {
    SCOPED_TRACE("Short bookmarks for long inputs enabled.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        omnibox::kShortBookmarkSuggestionsByTotalInputLength);
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 1, trigger_feature);
    TestNumMatchesAndTriggeredFeature("tes", 1, trigger_feature);
    TestNumMatchesAndTriggeredFeature("te sh bo", 1, trigger_feature);
  }

  {
    SCOPED_TRACE("Short bookmarks for long inputs enabled with threshold 5.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kShortBookmarkSuggestionsByTotalInputLength,
        {{OmniboxFieldTrial::
              kShortBookmarkSuggestionsByTotalInputLengthThreshold.name,
          "5"}});
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0);
    TestNumMatchesAndTriggeredFeature("te   ", 1, trigger_feature);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 1, trigger_feature);
  }

  {
    SCOPED_TRACE("Short bookmarks for long inputs counterfactual.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kShortBookmarkSuggestionsByTotalInputLength,
        {{OmniboxFieldTrial::
              kShortBookmarkSuggestionsByTotalInputLengthCounterfactual.name,
          "true"}});
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0, trigger_feature);
    TestNumMatchesAndTriggeredFeature("tes", 1, trigger_feature);
    TestNumMatchesAndTriggeredFeature("te sh bo", 0, trigger_feature);
  }

  {
    SCOPED_TRACE(
        "Short bookmarks for long inputs counterfactual with threshold 5.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kShortBookmarkSuggestionsByTotalInputLength,
        {{OmniboxFieldTrial::
              kShortBookmarkSuggestionsByTotalInputLengthThreshold.name,
          "5"},
         {OmniboxFieldTrial::
              kShortBookmarkSuggestionsByTotalInputLengthCounterfactual.name,
          "true"}});
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0);
    TestNumMatchesAndTriggeredFeature("te   ", 0, trigger_feature);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 0, trigger_feature);
  }

  {
    SCOPED_TRACE("Shortcut non-prefix rich autocompletion enabled.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {{OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesMinChar.name,
          "4"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixMinChar
              .name,
          "5"},
         {OmniboxFieldTrial::
              kRichAutocompletionAutocompleteNonPrefixShortcutProvider.name,
          "true"}});
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0);
    TestNumMatchesAndTriggeredFeature("te   ", 0);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 0);
  }

  {
    SCOPED_TRACE("Non-prefix rich autocompletion enabled with limit 5.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {{OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesMinChar.name,
          "4"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixMinChar
              .name,
          "5"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixAll.name,
          "true"}});
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0);
    TestNumMatchesAndTriggeredFeature("te   ", 1, trigger_feature);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 1, trigger_feature);
  }

  {
    SCOPED_TRACE("Title rich autocompletion enabled with limit 4.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {{OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesMinChar.name,
          "4"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixMinChar
              .name,
          "5"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteTitles.name,
          "true"}});
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0);
    TestNumMatchesAndTriggeredFeature("te  ", 1, trigger_feature);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 1, trigger_feature);
  }

  {
    SCOPED_TRACE(
        "Title and non-prefix rich autocompletion enabled with limits 4 and "
        "5.");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {{OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesMinChar.name,
          "4"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixMinChar
              .name,
          "5"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixMinChar
              .name,
          "true"},
         {OmniboxFieldTrial::kRichAutocompletionAutocompleteTitles.name,
          "true"}});
    TestNumMatchesAndTriggeredFeature("te", 0);
    TestNumMatchesAndTriggeredFeature("te ", 0);
    TestNumMatchesAndTriggeredFeature("te  ", 1, trigger_feature);
    TestNumMatchesAndTriggeredFeature("tes", 1);
    TestNumMatchesAndTriggeredFeature("te sh bo", 1, trigger_feature);
  }
}

TEST_F(BookmarkProviderTest, GetMatchesWithBookmarkPaths) {
  auto trigger_feature =
      OmniboxTriggeredFeatureService::Feature::kBookmarkPaths;

  {
    // When the feature is off, should not return path matched bookmarks nor
    // trigger counterfactual logging.
    SCOPED_TRACE("feature disabled");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(omnibox::kBookmarkPaths);
    TestNumMatchesAndTriggeredFeature("carefully other", 0);
  }

  {
    // When enabled without counterfactual logging, should return path matched
    // bookmark but not trigger counterfactual logging even it path matched.
    SCOPED_TRACE("feature enabled without counterfactual");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kBookmarkPaths);
    TestNumMatchesAndTriggeredFeature("carefully other", 1);
  }

  {
    // When enabled with "control" counterfactual logging, should not return
    // path matched bookmarks but trigger counterfactual logging if it path
    // matched.
    SCOPED_TRACE("feature enabled with control counterfactual");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kBookmarkPaths,
        {{OmniboxFieldTrial::kBookmarkPathsCounterfactual.name, "control"}});
    TestNumMatchesAndTriggeredFeature("carefully", 1);
    TestNumMatchesAndTriggeredFeature("carefully other", 0, trigger_feature);
  }

  {
    // When enabled with "enabled" counterfactual logging, should return path
    // matched bookmarks and trigger counterfactual logging if it path
    // matched.
    SCOPED_TRACE("feature enabled with enabled counterfactual");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kBookmarkPaths,
        {{OmniboxFieldTrial::kBookmarkPathsCounterfactual.name, "enabled"}});
    TestNumMatchesAndTriggeredFeature("carefully", 1);
    TestNumMatchesAndTriggeredFeature("carefully other", 1, trigger_feature);
  }
}

// Make sure that user input is trimmed correctly for starter pack keyword mode.
// In this mode, suggestions should be provided for only the user input after
// the keyword, i.e. "@bookmarks domain" should only match "domain".
TEST_F(BookmarkProviderTest, KeywordModeExtractUserInput) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kSiteSearchStarterPack);

  // Populate template URL with starter pack entries
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      TemplateURLStarterPackData::GetStarterPackEngines();
  for (auto& turl : turls) {
    provider_client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }
  // Test result for user text "domain", we should get back a result for domain.
  AutocompleteInput input(u"domain", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->Start(input, false);

  ACMatches matches = provider_->matches();
  ASSERT_GT(matches.size(), 0u);
  EXPECT_EQ(u"domain", matches[0].description);

  // Test result for "@bookmarks" and "@bookmarks domain" while NOT in keyword
  // mode, we should get a result for the @bookmarks bookmark and not for the
  // domain bookmark since we're searching for the whole input text including
  // "@bookmarks".
  AutocompleteInput input2(u"@bookmarks", metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  provider_->Start(input2, false);

  matches = provider_->matches();
  ASSERT_GT(matches.size(), 0u);
  EXPECT_EQ(u"@bookmarks", matches[0].description);

  AutocompleteInput input3(u"@bookmarks domain",
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  provider_->Start(input3, false);

  matches = provider_->matches();
  ASSERT_EQ(matches.size(), 0u);

  // Turn on keyword mode, test result again, we should only get back the result
  // for the domain bookmark since we're searching only for the user text after
  // the keyword.
  input3.set_prefer_keyword(true);
  input3.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  provider_->Start(input3, false);

  matches = provider_->matches();
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(u"domain", matches[0].description);

  // Ensure keyword and transition are set properly to keep user in keyword
  // mode.
  EXPECT_TRUE(matches[0].from_keyword);
  EXPECT_EQ(matches[0].keyword, u"@bookmarks");
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[0].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
}

TEST_F(BookmarkProviderTest, MaxMatches) {
  // Keyword mode is off. We should only get provider_max_matches_ matches.
  AutocompleteInput input(u"zyx", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->Start(input, false);

  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), provider_->provider_max_matches());

  // Turn keyword mode on. we should be able to get more matches now.
  input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  input.set_prefer_keyword(true);
  provider_->Start(input, false);

  matches = provider_->matches();
  EXPECT_EQ(matches.size(), provider_->provider_max_matches_in_keyword_mode());
}
