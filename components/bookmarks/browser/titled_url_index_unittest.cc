// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_index.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/bookmarks/browser/typed_count_sorter.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/query_parser/query_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;

namespace bookmarks {
namespace {

// Helper to create vector of buckets. Each `pairs` are structured
// `{min sample, count}`.
std::vector<base::Bucket> CreateBuckets(
    const std::vector<std::pair<int, int>>& pairs) {
  std::vector<base::Bucket> buckets;
  for (const auto& pair : pairs)
    buckets.emplace_back(pair.first, pair.second);
  return buckets;
}

// Used for sorting in combination with TypedCountSorter.
class BookmarkClientMock : public TestBookmarkClient {
 public:
  explicit BookmarkClientMock(const std::map<GURL, int>& typed_count_map)
      : typed_count_map_(typed_count_map) {}

  BookmarkClientMock(const BookmarkClientMock&) = delete;
  BookmarkClientMock& operator=(const BookmarkClientMock&) = delete;

  bool SupportsTypedCountForUrls() override { return true; }

  void GetTypedCountForUrls(UrlTypedCountMap* url_typed_count_map) override {
    for (auto& url_typed_count_pair : *url_typed_count_map) {
      const GURL* url = url_typed_count_pair.first;
      if (!url)
        continue;

      auto found = typed_count_map_.find(*url);
      if (found == typed_count_map_.end())
        continue;

      url_typed_count_pair.second = found->second;
    }
  }

 private:
  const std::map<GURL, int> typed_count_map_;
};

// Minimal implementation of TitledUrlNode.
class TestTitledUrlNode : public TitledUrlNode {
 public:
  TestTitledUrlNode(const std::u16string& title,
                    const GURL& url,
                    const std::u16string& ancestor_title)
      : title_(title), url_(url), ancestor_title_(ancestor_title) {}

  ~TestTitledUrlNode() override = default;

  const std::u16string& GetTitledUrlNodeTitle() const override {
    return title_;
  }

  const GURL& GetTitledUrlNodeUrl() const override { return url_; }

  std::vector<base::StringPiece16> GetTitledUrlNodeAncestorTitles()
      const override {
    return {ancestor_title_};
  }

 private:
  std::u16string title_;
  GURL url_;
  std::u16string ancestor_title_;
};

class TitledUrlIndexTest : public testing::Test {
 public:
  const GURL kAboutBlankURL = GURL("about:blank");

  TitledUrlIndexTest() { ResetNodes(); }

  ~TitledUrlIndexTest() override = default;

  void ResetNodes() {
    index_ = std::make_unique<TitledUrlIndex>();
    owned_nodes_.clear();
  }

  TitledUrlNode* AddNode(const std::string& title,
                         const GURL& url,
                         const std::string& ancestor_title = "") {
    return AddNode(UTF8ToUTF16(title), url, UTF8ToUTF16(ancestor_title));
  }

  TitledUrlNode* AddNode(
      const std::u16string& title,
      const GURL& url,
      const std::u16string& ancestor_title = std::u16string()) {
    owned_nodes_.push_back(
        std::make_unique<TestTitledUrlNode>(title, url, ancestor_title));
    index_->Add(owned_nodes_.back().get());
    return owned_nodes_.back().get();
  }

  void AddNodes(const char** titles, const char** urls, size_t count) {
    for (size_t i = 0; i < count; ++i)
      AddNode(titles[i], GURL(urls[i]));
  }

  std::vector<TitledUrlMatch> GetResultsMatching(
      const std::string& query,
      size_t max_count,
      bool match_ancestor_titles = false) {
    return index_->GetResultsMatching(UTF8ToUTF16(query), max_count,
                                      query_parser::MatchingAlgorithm::DEFAULT,
                                      match_ancestor_titles);
  }

  void ExpectMatches(const std::string& query,
                     const char** expected_titles,
                     size_t expected_count) {
    std::vector<std::string> title_vector;
    for (size_t i = 0; i < expected_count; ++i)
      title_vector.push_back(expected_titles[i]);
    ExpectMatches(query, query_parser::MatchingAlgorithm::DEFAULT,
                  title_vector);
  }

  void ExpectMatches(const std::string& query,
                     query_parser::MatchingAlgorithm matching_algorithm,
                     const std::vector<std::string>& expected_titles) {
    std::vector<TitledUrlMatch> matches = index_->GetResultsMatching(
        UTF8ToUTF16(query), 1000, matching_algorithm, false);
    ASSERT_EQ(expected_titles.size(), matches.size());
    for (const std::string& expected_title : expected_titles) {
      bool found = false;
      for (size_t j = 0; j < matches.size(); ++j) {
        const std::u16string& title = matches[j].node->GetTitledUrlNodeTitle();
        if (UTF8ToUTF16(expected_title) == title) {
          matches.erase(matches.begin() + j);
          found = true;
          break;
        }
      }
      ASSERT_TRUE(found);
    }
  }

  void ExtractMatchPositions(const std::string& string,
                             TitledUrlMatch::MatchPositions* matches) {
    for (const base::StringPiece& match : base::SplitStringPiece(
             string, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      std::vector<base::StringPiece> chunks = base::SplitStringPiece(
          match, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      ASSERT_EQ(2U, chunks.size());
      matches->push_back(TitledUrlMatch::MatchPosition());
      int chunks0, chunks1;
      EXPECT_TRUE(base::StringToInt(chunks[0], &chunks0));
      EXPECT_TRUE(base::StringToInt(chunks[1], &chunks1));
      matches->back().first = chunks0;
      matches->back().second = chunks1;
    }
  }

  void ExpectMatchPositions(
      const TitledUrlMatch::MatchPositions& actual_positions,
      const TitledUrlMatch::MatchPositions& expected_positions) {
    ASSERT_EQ(expected_positions.size(), actual_positions.size());
    for (size_t i = 0; i < expected_positions.size(); ++i) {
      EXPECT_EQ(expected_positions[i].first, actual_positions[i].first);
      EXPECT_EQ(expected_positions[i].second, actual_positions[i].second);
    }
  }

  TitledUrlIndex* index() { return index_.get(); }

 private:
  std::vector<std::unique_ptr<TestTitledUrlNode>> owned_nodes_;
  std::unique_ptr<TitledUrlIndex> index_;
};

// Various permutations with differing input, queries and output that exercises
// all query paths.
TEST_F(TitledUrlIndexTest, GetResultsMatching) {
  struct TestData {
    const std::string titles;
    const std::string query;
    const std::string expected;
  } data[] = {
      // Trivial test case of only one term, exact match.
      {"a;b", "A", "a"},

      // Two terms, exact matches.
      {"a b;b", "a b", "a b"},

      // Prefix match, one term.
      {"abcd;abc;b", "abc", "abcd;abc"},

      // Prefix match, multiple terms.
      {"abcd cdef;abcd;abcd cdefg", "abc cde", "abcd cdef;abcd cdefg"},

      // Exact and prefix match.
      {"ab cdef;abcd;abcd cdefg", "ab cdef", "ab cdef"},

      // Exact and prefix match.
      {"ab cdef ghij;ab;cde;cdef;ghi;cdef ab;ghij ab", "ab cde ghi",
       "ab cdef ghij"},

      // Title with term multiple times.
      {"ab ab", "ab", "ab ab"},

      // Make sure quotes don't do a prefix match.
      {"think", "\"thi\"", ""},

      // Prefix matches against multiple candidates.
      {"abc1 abc2 abc3 abc4", "abc", "abc1 abc2 abc3 abc4"},

      // Multiple prefix matches (with a lot of redundancy) against multiple
      // candidates.
      {"abc1 abc2 abc3 abc4 def1 def2 def3 def4",
       "abc def abc def abc def abc def abc def",
       "abc1 abc2 abc3 abc4 def1 def2 def3 def4"},

      // Prefix match on the first term.
      {"abc", "a", ""},

      // Prefix match on subsequent terms.
      {"abc def", "abc d", ""},
  };
  for (const TestData& test_data : data) {
    SCOPED_TRACE("Query: " + test_data.query);
    ResetNodes();

    for (const std::string& title :
         base::SplitString(test_data.titles, ";", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)) {
      AddNode(title, kAboutBlankURL);
    }

    std::vector<std::string> expected;
    if (!test_data.expected.empty()) {
      expected = base::SplitString(test_data.expected, ";",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    }

    ExpectMatches(test_data.query, query_parser::MatchingAlgorithm::DEFAULT,
                  expected);
  }
}

TEST_F(TitledUrlIndexTest, GetResultsMatchingAlwaysPrefixSearch) {
  struct TestData {
    const std::string titles;
    const std::string query;
    const std::string expected;
  } data[] = {
      // Trivial test case of only one term, exact match.
      {"z;y", "Z", "z"},

      // Prefix match, one term.
      {"abcd;abc;b", "abc", "abcd;abc"},

      // Prefix match, multiple terms.
      {"abcd cdef;abcd;abcd cdefg", "abc cde", "abcd cdef;abcd cdefg"},

      // Exact and prefix match.
      {"ab cdef ghij;ab;cde;cdef;ghi;cdef ab;ghij ab", "ab cde ghi",
       "ab cdef ghij"},

      // Title with term multiple times.
      {"ab ab", "ab", "ab ab"},

      // Make sure quotes don't do a prefix match.
      {"think", "\"thi\"", ""},

      // Prefix matches against multiple candidates.
      {"abc1 abc2 abc3 abc4", "abc", "abc1 abc2 abc3 abc4"},

      // Prefix match on the first term.
      {"abc", "a", "abc"},

      // Prefix match on subsequent terms.
      {"abc def", "abc d", "abc def"},

      // Exact and prefix match.
      {"ab cdef;abcd;abcd cdefg", "ab cdef", "ab cdef;abcd cdefg"},
  };
  for (const TestData& test_data : data) {
    ResetNodes();

    for (const std::string& title :
         base::SplitString(test_data.titles, ";", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)) {
      AddNode(title, kAboutBlankURL);
    }

    std::vector<std::string> expected;
    if (!test_data.expected.empty()) {
      expected = base::SplitString(test_data.expected, ";",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    }

    ExpectMatches(test_data.query,
                  query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH,
                  expected);
  }
}

// Analogous to GetResultsMatching, this test tests various permutations
// of title, URL, and input to see if the title/URL matches the input as
// expected.
TEST_F(TitledUrlIndexTest, GetResultsMatchingWithURLs) {
  struct TestData {
    const std::string query;
    const std::string title;
    const std::string url;
    const bool should_be_retrieved;
  } data[] = {
      // Test single-word inputs.  Include both exact matches and prefix
      // matches.
      {"foo", "Foo", "http://www.bar.com/", true},
      {"foo", "Foodie", "http://www.bar.com/", true},
      {"foo", "Bar", "http://www.foo.com/", true},
      {"foo", "Bar", "http://www.foodie.com/", true},
      {"foo", "Foo", "http://www.foo.com/", true},
      {"foo", "Bar", "http://www.bar.com/", false},
      {"foo", "Bar", "http://www.bar.com/blah/foo/blah-again/ ", true},
      {"foo", "Bar", "http://www.bar.com/blah/foodie/blah-again/ ", true},
      {"foo", "Bar", "http://www.bar.com/blah-foo/blah-again/ ", true},
      {"foo", "Bar", "http://www.bar.com/blah-foodie/blah-again/ ", true},
      {"foo", "Bar", "http://www.bar.com/blahafoo/blah-again/ ", false},

      // Test multi-word inputs.
      {"foo bar", "Foo Bar", "http://baz.com/", true},
      {"foo bar", "Foodie Bar", "http://baz.com/", true},
      {"bar foo", "Foo Bar", "http://baz.com/", true},
      {"bar foo", "Foodie Barly", "http://baz.com/", true},
      {"foo bar", "Foo Baz", "http://baz.com/", false},
      {"foo bar", "Foo Baz", "http://bar.com/", true},
      {"foo bar", "Foo Baz", "http://barly.com/", true},
      {"foo bar", "Foodie Baz", "http://barly.com/", true},
      {"bar foo", "Foo Baz", "http://bar.com/", true},
      {"bar foo", "Foo Baz", "http://barly.com/", true},
      {"foo bar", "Baz Bar", "http://blah.com/foo", true},
      {"foo bar", "Baz Barly", "http://blah.com/foodie", true},
      {"foo bar", "Baz Bur", "http://blah.com/foo/bar", true},
      {"foo bar", "Baz Bur", "http://blah.com/food/barly", true},
      {"foo bar", "Baz Bur", "http://bar.com/blah/foo", true},
      {"foo bar", "Baz Bur", "http://barly.com/blah/food", true},
      {"foo bar", "Baz Bur", "http://bar.com/blah/flub", false},
      {"foo bar", "Baz Bur", "http://foo.com/blah/flub", false}};

  for (const TestData& test_data : data) {
    ResetNodes();
    AddNode(test_data.title, GURL(test_data.url));

    std::vector<std::string> expected;
    if (test_data.should_be_retrieved)
      expected.push_back(test_data.title);

    ExpectMatches(test_data.query, query_parser::MatchingAlgorithm::DEFAULT,
                  expected);
  }
}

TEST_F(TitledUrlIndexTest, Normalization) {
  struct TestData {
    const char* const title;
    const char* const query;
  } data[] = {{"fooa\xcc\x88-test", "foo\xc3\xa4-test"},
              {"fooa\xcc\x88-test", "fooa\xcc\x88-test"},
              {"fooa\xcc\x88-test", "foo\xc3\xa4"},
              {"fooa\xcc\x88-test", "fooa\xcc\x88"},
              {"fooa\xcc\x88-test", "foo"},
              {"foo\xc3\xa4-test", "foo\xc3\xa4-test"},
              {"foo\xc3\xa4-test", "fooa\xcc\x88-test"},
              {"foo\xc3\xa4-test", "foo\xc3\xa4"},
              {"foo\xc3\xa4-test", "fooa\xcc\x88"},
              {"foo\xc3\xa4-test", "foo"},
              {"foo", "foo"}};

  for (const TestData& test_data : data) {
    ResetNodes();
    AddNode(test_data.title, kAboutBlankURL);

    std::vector<TitledUrlMatch> matches =
        GetResultsMatching(test_data.query, 10);
    EXPECT_EQ(1u, matches.size());
  }
}

// Makes sure match positions are updated appropriately for title matches.
TEST_F(TitledUrlIndexTest, MatchPositionsTitles) {
  struct TestData {
    const std::string title;
    const std::string query;
    const std::string expected_title_match_positions;
  } data[] = {
      // Trivial test case of only one term, exact match.
      {"a", "A", "0,1"},
      {"foo bar", "bar", "4,7"},
      {"fooey bark", "bar foo", "0,3:6,9"},
      // Non-trivial tests.
      {"foobar foo", "foobar foo", "0,6:7,10"},
      {"foobar foo", "foo foobar", "0,6:7,10"},
      {"foobar foobar", "foobar foo", "0,6:7,13"},
      {"foobar foobar", "foo foobar", "0,6:7,13"},
  };
  for (const TestData& test_data : data) {
    ResetNodes();
    AddNode(test_data.title, kAboutBlankURL);

    std::vector<TitledUrlMatch> matches =
        GetResultsMatching(test_data.query, 1000);
    ASSERT_EQ(1U, matches.size());

    TitledUrlMatch::MatchPositions expected_title_matches;
    ExtractMatchPositions(test_data.expected_title_match_positions,
                          &expected_title_matches);
    ExpectMatchPositions(matches[0].title_match_positions,
                         expected_title_matches);
  }
}

// Makes sure match positions are updated appropriately for URL matches.
TEST_F(TitledUrlIndexTest, MatchPositionsURLs) {
  // The encoded stuff between /wiki/ and the # is 第二次世界大戦
  const std::string ja_wiki_url =
      "http://ja.wikipedia.org/wiki/%E7%AC%AC%E4"
      "%BA%8C%E6%AC%A1%E4%B8%96%E7%95%8C%E5%A4%A7%E6%88%A6#.E3.83.B4.E3.82.A7"
      ".E3.83.AB.E3.82.B5.E3.82.A4.E3.83.A6.E4.BD.93.E5.88.B6";
  struct TestData {
    const std::string query;
    const std::string url;
    const std::string expected_url_match_positions;
  } data[] = {
      {"foo", "http://www.foo.com/", "11,14"},
      {"foo", "http://www.foodie.com/", "11,14"},
      {"foo", "http://www.foofoo.com/", "11,14"},
      {"www", "http://www.foo.com/", "7,10"},
      {"foo", "http://www.foodie.com/blah/foo/fi", "11,14:27,30"},
      {"foo", "http://www.blah.com/blah/foo/fi", "25,28"},
      {"foo www", "http://www.foodie.com/blah/foo/fi", "7,10:11,14:27,30"},
      {"www foo", "http://www.foodie.com/blah/foo/fi", "7,10:11,14:27,30"},
      {"www bla", "http://www.foodie.com/blah/foo/fi", "7,10:22,25"},
      {"http", "http://www.foo.com/", "0,4"},
      {"http www", "http://www.foo.com/", "0,4:7,10"},
      {"http foo", "http://www.foo.com/", "0,4:11,14"},
      {"http foo", "http://www.bar.com/baz/foodie/hi", "0,4:23,26"},
      {"第二次", ja_wiki_url, "29,56"},
      {"ja 第二次", ja_wiki_url, "7,9:29,56"},
      {"第二次 E3.8", ja_wiki_url,
       "29,56:94,98:103,107:"
       "112,116:121,125:"
       "130,134:139,143"}};

  for (const TestData& test_data : data) {
    ResetNodes();
    AddNode("123456", GURL(test_data.url));

    std::vector<TitledUrlMatch> matches =
        GetResultsMatching(test_data.query, 1000);
    ASSERT_EQ(1U, matches.size()) << test_data.url << test_data.query;

    TitledUrlMatch::MatchPositions expected_url_matches;
    ExtractMatchPositions(test_data.expected_url_match_positions,
                          &expected_url_matches);
    ExpectMatchPositions(matches[0].url_match_positions, expected_url_matches);
  }
}

// Makes sure index is updated when a node is removed.
TEST_F(TitledUrlIndexTest, Remove) {
  TitledUrlNode* n1 = AddNode("foo", GURL("http://foo"));
  TitledUrlNode* n2 = AddNode("bar", GURL("http://bar"));
  TitledUrlNode* n3 = AddNode("bar", GURL("http://bar/baz"));

  ASSERT_EQ(1U, GetResultsMatching("foo", 10).size());
  ASSERT_EQ(2U, GetResultsMatching("bar", 10).size());

  index()->Remove(n3);
  EXPECT_EQ(1U, GetResultsMatching("bar", 10).size());
  EXPECT_EQ(1U, GetResultsMatching("foo", 10).size());

  index()->Remove(n1);
  EXPECT_EQ(1U, GetResultsMatching("bar", 10).size());
  EXPECT_EQ(0U, GetResultsMatching("foo", 10).size());

  index()->Remove(n2);
  EXPECT_EQ(0U, GetResultsMatching("bar", 10).size());
}

// Makes sure no more than max queries is returned.
TEST_F(TitledUrlIndexTest, HonorMax) {
  AddNode("abcd", kAboutBlankURL);
  AddNode("abcde", kAboutBlankURL);

  EXPECT_EQ(1U, GetResultsMatching("ABc", 1).size());
}

// Makes sure if the lower case string of a bookmark title is more characters
// than the upper case string no match positions are returned.
TEST_F(TitledUrlIndexTest, EmptyMatchOnMultiwideLowercaseString) {
  TitledUrlNode* n1 = AddNode(u"\u0130 i", GURL("http://www.google.com"));

  std::vector<TitledUrlMatch> matches = GetResultsMatching("i", 100);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(n1, matches[0].node);
  EXPECT_TRUE(matches[0].title_match_positions.empty());
}

TEST_F(TitledUrlIndexTest, GetResultsSortedByTypedCount) {
  struct TestData {
    const GURL url;
    const char* title;
    const int typed_count;
  } data[] = {
      {GURL("http://www.google.com/"), "Google", 100},
      {GURL("http://maps.google.com/"), "Google Maps", 40},
      {GURL("http://docs.google.com/"), "Google Docs", 50},
      {GURL("http://reader.google.com/"), "Google Reader", 80},
  };

  std::map<GURL, int> typed_count_map;
  for (const TestData& test_data : data)
    typed_count_map.insert(
        std::make_pair(test_data.url, test_data.typed_count));

  BookmarkClientMock bookmark_client(typed_count_map);
  index()->SetNodeSorter(std::make_unique<TypedCountSorter>(&bookmark_client));

  for (const TestData& test_data : data)
    AddNode(test_data.title, GURL(test_data.url));

  // Populate match nodes.
  std::vector<TitledUrlMatch> matches = GetResultsMatching("google", 4);

  // The resulting order should be:
  // 1. Google (google.com) 100
  // 2. Google Reader (google.com/reader) 80
  // 3. Google Docs (docs.google.com) 50
  // 4. Google Maps (maps.google.com) 40
  ASSERT_EQ(4U, matches.size());
  EXPECT_EQ(data[0].url, matches[0].node->GetTitledUrlNodeUrl());
  EXPECT_EQ(data[3].url, matches[1].node->GetTitledUrlNodeUrl());
  EXPECT_EQ(data[2].url, matches[2].node->GetTitledUrlNodeUrl());
  EXPECT_EQ(data[1].url, matches[3].node->GetTitledUrlNodeUrl());

  // Select top two matches.
  matches = GetResultsMatching("google", 2);

  ASSERT_EQ(2U, matches.size());
  EXPECT_EQ(data[0].url, matches[0].node->GetTitledUrlNodeUrl());
  EXPECT_EQ(data[3].url, matches[1].node->GetTitledUrlNodeUrl());
}

TEST_F(TitledUrlIndexTest, RetrieveNodesMatchingAllTerms) {
  TitledUrlNode* node =
      AddNode("term1 term2 other xyz ab", GURL("http://foo.com"));

  struct TestData {
    const std::string query;
    const bool should_be_retrieved;
  } data[] = {// Should return matches if all input terms match, even if not all
              // node terms match.
              {"term other", true},
              // Should not match midword.
              {"term ther", false},
              // Short input terms should only return exact matches.
              {"xy", false},
              {"ab", true}};

  for (const TestData& test_data : data) {
    SCOPED_TRACE("Query: " + test_data.query);
    std::vector<std::u16string> terms =
        base::SplitString(base::UTF8ToUTF16(test_data.query), u" ",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    auto matches = index()->RetrieveNodesMatchingAllTermsForTesting(
        terms, query_parser::MatchingAlgorithm::DEFAULT);
    if (test_data.should_be_retrieved) {
      EXPECT_EQ(matches.size(), 1u);
      EXPECT_TRUE(matches.contains(node));
    } else
      EXPECT_TRUE(matches.empty());
  };
}

TEST_F(TitledUrlIndexTest, RetrieveNodesMatchingAnyTerms) {
  TitledUrlNode* node =
      AddNode("term1 term2 other xyz ab", GURL("http://foo.com"));

  struct TestData {
    const std::string query;
    const bool should_be_retrieved;
    const int histogram_terms_unioned_count;
    const std::vector<std::pair<int, int>> histogram_term_node_counts;
    const int histogram_any_terms_nodes;
    const int histogram_all_terms_nodes;
    const int histogram_joint_nodes;
  } data[] = {
      // Should return matches if any input terms match, even if not all node
      // terms match.
      {"term not", true, 1, {{0, 1}, {2, 1}}, 1, 0, 1},
      // Should not return duplicate matches.
      {"term term1 term2", true, 3, {{1, 2}, {2, 1}}, 1, 0, 1},
      // Should not early exit when there are no intermediate matches.
      {"not term", true, 1, {{0, 1}, {2, 1}}, 1, 0, 1},
      // Should not match midword.
      {"ther", false, 0, {{0, 1}}, 0, 0, 0},
      // Short input terms should only return exact matches.
      {"xy", false, 0, {{0, 1}}, 0, 0, 0},
      {"ab", true, 1, {{1, 1}}, 1, 0, 1}};

  for (const TestData& test_data : data) {
    SCOPED_TRACE("Query: " + test_data.query);
    base::HistogramTester histogram_tester;
    std::vector<std::u16string> terms =
        base::SplitString(base::UTF8ToUTF16(test_data.query), u" ",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    auto matches = index()->RetrieveNodesMatchingAnyTermsForTesting(
        terms, query_parser::MatchingAlgorithm::DEFAULT, 9);

    // Verify whether the node matched.
    if (test_data.should_be_retrieved) {
      EXPECT_EQ(matches.size(), 1u);
      EXPECT_TRUE(matches.contains(node));
    } else
      EXPECT_TRUE(matches.empty());

    // Verify histograms.
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.AnyTermApproach.TermsUnionedCount",
        test_data.histogram_terms_unioned_count, 1);
    EXPECT_EQ(
        CreateBuckets(test_data.histogram_term_node_counts),
        histogram_tester.GetAllSamples(
            "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountPerTerm"));
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAnyTerms",
        test_data.histogram_any_terms_nodes, 1);
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAllTerms",
        test_data.histogram_all_terms_nodes, 1);
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCount",
        test_data.histogram_joint_nodes, 1);
  };
}

TEST_F(TitledUrlIndexTest, RetrieveNodesMatchingAnyTermsMaxNodes) {
  std::vector<TitledUrlNode*> nodes = {
      AddNode("common11", GURL("http://foo.com")),
      AddNode("common12", GURL("http://foo.com")),
      AddNode("common13 uncommon", GURL("http://foo.com")),
      AddNode("common21 uncommon1", GURL("http://foo.com")),
      AddNode("common22 uncommon1", GURL("http://foo.com")),
      AddNode("common23 uncommon1", GURL("http://foo.com")),
  };

  struct TestData {
    const std::string query;
    const std::vector<int> retrieved_node_indexes;
    const bool histogram_any_term_approach_used;
    const int histogram_terms_unioned_count;
    const std::vector<std::pair<int, int>> histogram_term_node_counts;
    const int histogram_any_terms_nodes;
    const int histogram_all_terms_nodes;
    const int histogram_joint_nodes;
  } data[] = {
      // Should not look for all-term matches if at least 1 term matches at most
      // `max_nodes`.
      {"uncommon1", {3, 4, 5}, true, 1, {{3, 1}}, 3, 0, 3},
      // Like above, but even if some terms match more than `max_nodes`. Should
      // look for per term matches even after `max_nodes` matches have been
      // found.
      {"common uncommon1", {3, 4, 5}, true, 2, {{3, 1}, {6, 1}}, 3, 0, 3},
      // Should look for all-term matches if all terms match more than
      // `ma_nodes`.
      {"uncommon", {2, 3, 4, 5}, true, 1, {{4, 1}}, 3, 4, 4},
      {"common", {0, 1, 2, 3, 4, 5}, true, 1, {{6, 1}}, 3, 6, 6},
      {"common uncommon", {2, 3, 4, 5}, true, 2, {{4, 1}, {6, 1}}, 3, 4, 4},
      {"common x", {0, 1, 2}, true, 1, {{0, 1}, {6, 1}}, 3, 0, 3},
      // Should not crash if no term has matches.
      {"x", {}, true, 0, {{0, 1}}, 0, 0, 0},
  };

  for (const TestData& test_data : data) {
    SCOPED_TRACE("Query: " + test_data.query);
    base::HistogramTester histogram_tester;
    std::vector<std::u16string> terms =
        base::SplitString(base::UTF8ToUTF16(test_data.query), u" ",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    auto matches = index()->RetrieveNodesMatchingAnyTermsForTesting(
        terms, query_parser::MatchingAlgorithm::DEFAULT, 3);

    // Verify the correct nodes matched.
    ASSERT_EQ(matches.size(), test_data.retrieved_node_indexes.size());
    for (int index : test_data.retrieved_node_indexes) {
      SCOPED_TRACE("node: " +
                   base::UTF16ToUTF8(nodes[index]->GetTitledUrlNodeTitle()));
      EXPECT_TRUE(matches.contains(nodes[index]));
    }

    // Verify histograms.
    if (test_data.histogram_any_term_approach_used) {
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.AnyTermApproach.TermsUnionedCount",
          test_data.histogram_terms_unioned_count, 1);
      EXPECT_EQ(
          CreateBuckets(test_data.histogram_term_node_counts),
          histogram_tester.GetAllSamples(
              "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountPerTerm"));
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAnyTerms",
          test_data.histogram_any_terms_nodes, 1);
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAllTerms",
          test_data.histogram_all_terms_nodes, 1);
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCount",
          test_data.histogram_joint_nodes, 1);
    } else {
      // If AnyTermApproach wasn't used, then other histograms shouldn't be
      // recorded.
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.AnyTermApproach.TermsUnionedCount", 0);
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountPerTerm", 0);
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAnyTerms", 0);
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAllTerms", 0);
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCount", 0);
    }
  };
}

TEST_F(TitledUrlIndexTest, GetResultsMatchingAncestors) {
  TitledUrlNode* node = AddNode("leaf pare", GURL("http://foo.com"), "parent");

  struct TestData {
    const std::string query;
    const bool match_ancestor_titles;
    const bool should_be_retrieved;
    const bool should_have_ancestor_match;
    const bool histogram_any_term_approach_used;
    const int histogram_terms_count;
    const std::vector<std::pair<int, int>> histogram_term_lengths;
    const bool histogram_matched_node;
  } data[] = {
      // Should exclude matches with ancestor matches when
      // |match_ancestor_titles| is false.
      {"leaf parent", false, false, false, false, 2, {{4, 1}, {6, 1}}, false},
      // Should allow ancestor matches when |match_ancestor_titles| is true.
      {"leaf parent", true, true, true, true, 2, {{4, 1}, {6, 1}}, true},
      // Should not early exit when there are no accumulated
      // non-ancestor matches.
      {"parent leaf", true, true, true, true, 2, {{4, 1}, {6, 1}}, true},
      // Should still require at least 1 non-ancestor match when
      // |match_ancestor_titles| is true.
      {"parent", true, false, false, true, 1, {{6, 1}}, false},
      // Should set |has_ancestor_match| to true even if a term matched
      // both an ancestor and title/URL.
      {"pare", true, true, true, true, 1, {{4, 1}}, true},
      // Short inputs should only match exact title or ancestor terms.
      {"pa", true, false, false, true, 1, {{2, 1}}, false},
      // Should not return matches if a term matches neither the title
      // nor ancestor.
      {"leaf not parent",
       true,
       false,
       false,
       true,
       3,
       {{3, 1}, {4, 1}, {6, 1}},
       true}};

  for (const TestData& test_data : data) {
    SCOPED_TRACE("Query: " + test_data.query);
    base::HistogramTester histogram_tester;
    auto matches = GetResultsMatching(test_data.query, 10,
                                      test_data.match_ancestor_titles);

    // Verify whether the match.
    if (test_data.should_be_retrieved) {
      EXPECT_EQ(matches.size(), 1u);
      EXPECT_EQ(matches[0].node, node);
      EXPECT_EQ(matches[0].has_ancestor_match,
                test_data.should_have_ancestor_match);
    } else
      EXPECT_TRUE(matches.empty());

    // Verify histograms.
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.Terms.TermsCount",
        test_data.histogram_terms_count, 1);
    EXPECT_EQ(CreateBuckets(test_data.histogram_term_lengths),
              histogram_tester.GetAllSamples(
                  "Bookmarks.GetResultsMatching.Terms.TermLength"));
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.AnyTermApproach.Used",
        test_data.histogram_any_term_approach_used, 1);
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.Nodes.Count",
        test_data.histogram_matched_node, 1);
    if (test_data.query.size() < 3) {
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.Nodes.Count."
          "InputsShorterThan3CharsLong",
          test_data.histogram_matched_node, 1);
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.Nodes.Count.InputsAtLeast3CharsLong",
          0);
    } else {
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.Nodes.Count.InputsAtLeast3CharsLong",
          test_data.histogram_matched_node, 1);
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.Nodes.Count."
          "InputsShorterThan3CharsLong",
          0);
    }
    if (test_data.histogram_matched_node) {
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.Matches.ConsideredCount", 1, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.Matches.ConsideredCount", 0);
    }
    if (test_data.should_be_retrieved) {
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.Matches.ReturnedCount", 1, 1);
    } else if (test_data.histogram_matched_node) {
      histogram_tester.ExpectUniqueSample(
          "Bookmarks.GetResultsMatching.Matches.ReturnedCount", 0, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.Matches.ReturnedCount", 0);
    }
    EXPECT_EQ(histogram_tester
                  .GetAllSamples("Bookmarks.GetResultsMatching.Timing.Total")
                  .size(),
              1u);
    EXPECT_EQ(histogram_tester
                  .GetAllSamples(
                      "Bookmarks.GetResultsMatching.Timing.RetrievingNodes")
                  .size(),
              1u);
    if (test_data.histogram_matched_node) {
      EXPECT_EQ(
          histogram_tester
              .GetAllSamples("Bookmarks.GetResultsMatching.Timing.SortingNodes")
              .size(),
          1u);
      EXPECT_EQ(histogram_tester
                    .GetAllSamples(
                        "Bookmarks.GetResultsMatching.Timing.CreatingMatches")
                    .size(),
                1u);
    } else {
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.Timing.SortingNodes", 0);
      histogram_tester.ExpectTotalCount(
          "Bookmarks.GetResultsMatching.Timing.CreatingMatches", 0);
    }
  };

  {
    // With an empty input, most histograms should not be logged.
    base::HistogramTester histogram_tester;
    auto matches = GetResultsMatching("", 10, true);
    EXPECT_EQ(histogram_tester
                  .GetAllSamples("Bookmarks.GetResultsMatching.Timing.Total")
                  .size(),
              1u);
    histogram_tester.ExpectUniqueSample(
        "Bookmarks.GetResultsMatching.Terms.TermsCount", 0, 1);
    histogram_tester.ExpectTotalCount(
        "Bookmarks.GetResultsMatching.Terms.TermLength", 0);
    histogram_tester.ExpectTotalCount(
        "Bookmarks.GetResultsMatching.Timing.RetrievingNodes", 0);
  }
}

}  // namespace
}  // namespace bookmarks
