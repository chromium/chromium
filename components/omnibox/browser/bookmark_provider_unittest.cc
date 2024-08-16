// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/bookmark_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
    {"mno pqr short", "http://www.catsanddogs.com/f"},
    {"pqr mno loooong", "http://www.catsanddogs.com/g"},
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
  TestBookmarkPosition(size_t begin, size_t end) : begin(begin), end(end) {}

  bool operator==(const TestBookmarkPosition& other) const {
    return begin == other.begin && end == other.end;
  }

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

// Convenience function to make comparing ACMatchClassifications against the
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

}  // namespace

class BookmarkProviderTest : public testing::Test {
 public:
  BookmarkProviderTest();
  BookmarkProviderTest(const BookmarkProviderTest&) = delete;
  BookmarkProviderTest& operator=(const BookmarkProviderTest&) = delete;

 protected:
  void SetUp() override;

  // Starts fresh with a new BookmarkProvider.
  void ResetProvider();

  // Invokes |Start()| with |input_text| and returns the number of matches
  // provided.
  [[nodiscard]] size_t GetNumMatches(std::string input_text);

  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<MockAutocompleteProviderClient> provider_client_;
  std::unique_ptr<BookmarkModel> local_or_syncable_model_;
  scoped_refptr<BookmarkProvider> provider_;
  TestSchemeClassifier classifier_;
};

BookmarkProviderTest::BookmarkProviderTest() {
  local_or_syncable_model_ = bookmarks::TestBookmarkClient::CreateModel();
}

void BookmarkProviderTest::SetUp() {
  provider_client_ = std::make_unique<MockAutocompleteProviderClient>();
  ON_CALL(*provider_client_, GetBookmarkModel())
      .WillByDefault(testing::Return(local_or_syncable_model_.get()));
  ON_CALL(*provider_client_, GetSchemeClassifier())
      .WillByDefault(testing::ReturnRef(classifier_));

  provider_client_->set_template_url_service(
      search_engines_test_environment_.template_url_service());

  ResetProvider();

  const BookmarkNode* other_node = local_or_syncable_model_->other_node();
  for (size_t i = 0; i < std::size(bookmark_provider_test_data); ++i) {
    const BookmarksTestInfo& cur(bookmark_provider_test_data[i]);
    const GURL url(cur.url);
    local_or_syncable_model_->AddURL(other_node, other_node->children().size(),
                                     base::ASCIIToUTF16(cur.title), url);
  }
}

void BookmarkProviderTest::ResetProvider() {
  provider_ = new BookmarkProvider(provider_client_.get());
}

size_t BookmarkProviderTest::GetNumMatches(std::string input_text) {
  AutocompleteInput input(base::UTF8ToUTF16(input_text),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->Start(input, /*minimal_changes=*/false);
  return provider_->matches().size();
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
    std::string query;
    std::vector<TestBookmarkPositions> positions_per_match;
  };
  std::vector<QueryData> queries = {
      // This first set is primarily for position detection validation.
      {"abc", {{{0, 3}}, {{0, 3}}, {{0, 3}}}},
      {"abcde", {{{0, 5}}, {{0, 5}}}},
      {"foo bar", {}},
      {"fooey bark", {}},
      {"def", {{{2, 5}}, {{4, 7}}}},
      {"mno pqr", {{{0, 7}}, {{0, 3}, {4, 7}}}},
      // NB: GetBookmarksMatching(...) uses exact match for "a" in title or URL.
      {"a", {{{0, 1}}, {{0, 1}}}},
      {"a d", {{{0, 1}, {4, 5}}, {{0, 3}}}},
      {"carry carbon", {{{0, 12}}}},
      // NB: GetBookmarksMatching(...) sorts the match positions.
      {"carbon carry", {{{0, 5}, {6, 12}}}},
      {"arbon", {}},
      {"ar", {}},
      {"arry", {}},
      // Quoted terms are single terms.
      {"\"carry carbon\"", {{{0, 5}, {6, 12}}}},
      {"\"carry carbon\" care", {{{0, 5}, {6, 12}, {13, 17}}}},
      // Quoted terms require complete word matches.
      {"\"carry carbo\"", {}},
      // This set uses duplicated and/or overlaps search terms in the title.
      {"frank", {{{0, 5}}}},
      {"frankly", {{{0, 7}}}},
      {"frankly frankly", {{{0, 15}}}},
      {"foobar foo", {{{0, 10}}}},
      {"foo foobar", {{{0, 6}, {7, 13}}}},
      // This ensures that leading whitespace in the title is correctly offset.
      {"hello", {{{0, 5}}}},
      // This ensures that empty titles yield empty classifications.
      {"emptytitle", {{}}},
  };

  for (const auto& query : queries) {
    AutocompleteInput input(base::ASCIIToUTF16(query.query),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, /*minimal_changes=*/false);
    const ACMatches& matches(provider_->matches());
    // Validate number of results is as expected.
    EXPECT_LE(matches.size(), query.positions_per_match.size())
        << "One or more of the following matches were unexpected:\n"
        << MatchesAsString16(matches) << "For query '" << query.query << "'.";
    EXPECT_GE(matches.size(), query.positions_per_match.size())
        << "One or more expected matches are missing. Matches found:\n"
        << MatchesAsString16(matches) << "for query '" << query.query << "'.";
    // Validate positions within each match is as expected.
    for (size_t i = 0; i < matches.size(); ++i) {
      TestBookmarkPositions actual_positions(
          PositionsFromAutocompleteMatch(matches[i]));
      TestBookmarkPositions expected_positions = query.positions_per_match[i];
      EXPECT_EQ(actual_positions, expected_positions)
          << "EXPECTED: " << TestBookmarkPositionsAsString(expected_positions)
          << "ACTUAL:   " << TestBookmarkPositionsAsString(actual_positions)
          << "    for query: '" << query.query << "'.";
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
      {"abc",
       3,
       {"abcde",                // Most complete match.
        "abcdef", "abc def"}},  // Least complete match.
      {"ghi",
       2,
       {"ghi jkl",  // Matched earlier.
        "jkl ghi",  // Matched later.
        ""}},
      // Rankings of exact-word matches with different URLs.
      {"achlorhydric",
       3,
       {"achlorhydric mockingbirds resuscitates featherhead",
        "achlorhydric featherheads resuscitates mockingbirds",
        "featherhead resuscitates achlorhydric mockingbirds"}},
      {"achlorhydric featherheads",
       2,
       {"achlorhydric featherheads resuscitates mockingbirds",
        "mockingbirds resuscitates featherheads achlorhydric", ""}},
      {"mockingbirds resuscitates",
       3,
       {"mockingbirds resuscitates featherheads achlorhydric",
        "achlorhydric mockingbirds resuscitates featherhead",
        "featherhead resuscitates achlorhydric mockingbirds"}},
      // Ranking of exact-word matches with URL boosts.
      {"worms",
       2,
       {"burning worms #1",  // boosted
        "burning worms #2",  // not boosted
        ""}},
      // Ranking of prefix matches with URL boost.
      {"burn worm",
       3,
       {"burning worms #1",    // boosted
        "worming burns #10",   // boosted but longer title
        "burning worms #2"}},  // not boosted
      // A query of "worm burn" will have the same results.
      {"worm burn",
       3,
       {"burning worms #1",    // boosted
        "worming burns #10",   // boosted but longer title
        "burning worms #2"}},  // not boosted
  };

  for (size_t i = 0; i < std::size(query_data); ++i) {
    AutocompleteInput input(base::ASCIIToUTF16(query_data[i].query),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, /*minimal_changes=*/false);
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
    BookmarkNode node(/*id=*/0, base::Uuid::GenerateRandomV4(),
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
    provider_->Start(input, /*minimal_changes=*/false);
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
              match.contents)
        << description;
    std::vector<std::string> class_strings =
        base::SplitString(query_data[i].expected_contents_class, ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(class_strings.size(), match.contents_class.size()) << description;
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
  provider_->Start(input, /*minimal_changes=*/false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(BookmarkProviderTest, ShortBookmarks) {
  // Test the 2 short bookmark features that determine when short inputs should
  // be allowed to prefix match. These tests are trying to match the mock
  // bookmark "testing short bookmarks".

  EXPECT_EQ(GetNumMatches("te"), 0u);
  EXPECT_EQ(GetNumMatches("te "), 1u);
  EXPECT_EQ(GetNumMatches("tes"), 1u);
  EXPECT_EQ(GetNumMatches("te sh bo"), 1u);
}

TEST_F(BookmarkProviderTest, GetMatchesWithBookmarkPaths) {
  // Should return path matched bookmark.
  SCOPED_TRACE("feature enabled without counterfactual");
  EXPECT_EQ(GetNumMatches("carefully other"), 1u);
}

// Make sure that user input is trimmed correctly for starter pack keyword mode.
// In this mode, suggestions should be provided for only the user input after
// the keyword, i.e. "@bookmarks domain" should only match "domain".
TEST_F(BookmarkProviderTest, KeywordModeExtractUserInput) {
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
  provider_->Start(input, /*minimal_changes=*/false);

  ACMatches matches = provider_->matches();
  ASSERT_GT(matches.size(), 0u);
  EXPECT_EQ(matches[0].description, u"domain");

  // Test result for "@bookmarks" while NOT in keyword mode, we shouldn't get a
  // result because the input starts with "@".
  AutocompleteInput input2(u"@bookmarks", metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  provider_->Start(input2, /*minimal_changes=*/false);
  EXPECT_TRUE(provider_->matches().empty());

  // Test result for "domain @bookmarks" while NOT in keyword mode, we should
  // get a result. Although the input contains "@", it doesn't start with it.
  AutocompleteInput input3(u"domain @bookmarks",
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  provider_->Start(input3, /*minimal_changes=*/false);
  EXPECT_EQ(provider_->matches().size(), 1u);
  EXPECT_EQ(matches[0].description, u"domain");

  // In keyword mode, "@bookmarks domain" should match since we're only trying
  // to match "domain".
  AutocompleteInput input4(u"@bookmarks domain",
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  input4.set_prefer_keyword(true);
  input4.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  provider_->Start(input4, /*minimal_changes=*/false);

  matches = provider_->matches();
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].description, u"domain");

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
  provider_->Start(input, /*minimal_changes=*/false);

  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), provider_->provider_max_matches());

  // Turn keyword mode on. we should be able to get more matches now.
  input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  input.set_prefer_keyword(true);
  provider_->Start(input, /*minimal_changes=*/false);

  matches = provider_->matches();
  EXPECT_EQ(matches.size(), provider_->provider_max_matches_in_keyword_mode());

  // The provider should not limit the number of suggestions when ML scoring
  // w/increased candidates is enabled. Any matches beyond the limit should be
  // marked as culled_by_provider and have a relevance of 0.
  input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_INVALID);
  input.set_prefer_keyword(false);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{omnibox::kUrlScoringModel, {}},
       {omnibox::kMlUrlScoring,
        {{"MlUrlScoringUnlimitedNumCandidates", "true"}}}},
      /*disabled_features=*/{});
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;

  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 9u);
  // Matches below the `max_matches` limit.
  for (size_t i = 0; i < provider_->provider_max_matches(); i++) {
    EXPECT_FALSE(matches[i].culled_by_provider);
    EXPECT_GT(matches[i].relevance, 0);
  }
  // "Extra" matches above the `max_matches` limit. Should have 0 relevance and
  // be marked as `culled_by_provider`.
  for (size_t i = provider_->provider_max_matches(); i < matches.size(); i++) {
    EXPECT_TRUE(matches[i].culled_by_provider);
    EXPECT_EQ(matches[i].relevance, 0);
  }

  // Unlimited matches should ignore the provider max matches, even if the
  // `kMlUrlScoringMaxMatchesByProvider` param is set.
  scoped_ml_config.GetMLConfig().ml_url_scoring_max_matches_by_provider = "*:6";

  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 9u);
}
