// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Extensible data structure representing the fields of a search item.
struct ItemData {
  std::u16string title;
  std::u16string secondary_text = u"";
  std::vector<std::u16string> synonyms;
};

// In-memory fake implementation of FuzzySearchItem for unit testing.
class FakeFuzzySearchItem : public FuzzySearchItem {
 public:
  explicit FakeFuzzySearchItem(std::u16string title,
                               std::u16string secondary_text = u"",
                               std::vector<std::u16string> synonyms = {})
      : title_(std::move(title)),
        secondary_text_(std::move(secondary_text)),
        synonyms_(std::move(synonyms)) {}

  ~FakeFuzzySearchItem() override = default;

  const std::u16string& GetTitle() const override { return title_; }
  const std::u16string& GetSecondaryText() const override {
    return secondary_text_;
  }
  const std::vector<std::u16string>& GetSynonyms() const override {
    return synonyms_;
  }

 private:
  std::u16string title_;
  std::u16string secondary_text_;
  std::vector<std::u16string> synonyms_;
};

class FuzzyFinderTest : public testing::Test {
 public:
  // Helper to create and manage the lifetime of FakeFuzzySearchItems.
  std::vector<FuzzySearchItem*> CreateItems(
      const std::vector<ItemData>& items_data) {
    std::vector<FuzzySearchItem*> raw_items;
    raw_items.reserve(items_data.size());
    for (const auto& data : items_data) {
      storage_.push_back(std::make_unique<FakeFuzzySearchItem>(
          data.title, data.secondary_text, data.synonyms));
      raw_items.push_back(storage_.back().get());
    }
    return raw_items;
  }

  // Helper to extract titles from results for clean assertions.
  std::vector<std::u16string> ExtractResultTitles(
      const std::vector<FuzzySearchResult>& results) {
    std::vector<std::u16string> titles;
    titles.reserve(results.size());
    for (const auto& result : results) {
      if (result.item) {
        titles.push_back(result.item->GetTitle());
      }
    }
    return titles;
  }

 private:
  std::vector<std::unique_ptr<FakeFuzzySearchItem>> storage_;
};

TEST_F(FuzzyFinderTest, ExactFullMatch) {
  auto items = CreateItems({{u"New Tab"}, {u"New Window"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"New Tab", /*max_results=*/3);
  ASSERT_GE(results.size(), 1u);
  EXPECT_EQ(results[0].item->GetTitle(), u"New Tab");
  EXPECT_EQ(results[0].item, items[0]);
  if (results.size() > 1) {
    EXPECT_GT(results[0].score, results[1].score);
  }
}

TEST_F(FuzzyFinderTest, SubstringPrefixAndSuffixMatches) {
  auto items =
      CreateItems({{u"Open New Tab"}, {u"New Window"}, {u"Brand New"}});
  FuzzyFinder finder(items);

  // Prefix match
  auto results = finder.FuzzyFind(u"Open", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Open New Tab"));
  EXPECT_LE(results.size(), 4u);

  // Infix / substring match
  results = finder.FuzzyFind(u"New", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Open New Tab", u"New Window", u"Brand New"));
  EXPECT_LE(results.size(), 3u);

  // Suffix match
  results = finder.FuzzyFind(u"Window", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Window"));
  EXPECT_LE(results.size(), 1u);
}

TEST_F(FuzzyFinderTest, NoMatchReturnsEmpty) {
  auto items = CreateItems({{u"Bookmarks"}, {u"Downloads"}, {u"History"}});
  FuzzyFinder finder(items);

  EXPECT_THAT(finder.FuzzyFind(u"Settings", /*max_results=*/5), IsEmpty());
}

TEST_F(FuzzyFinderTest, QueryShorterThanTwoCharactersReturnsEmpty) {
  auto items = CreateItems({{u"Tab 1"}, {u"Bookmarks"}});
  FuzzyFinder finder(items);

  // 1-character queries return empty.
  EXPECT_THAT(finder.FuzzyFind(u"T", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.FuzzyFind(u"B", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.FuzzyFind(u"a", /*max_results=*/5), IsEmpty());

  // 2-character queries succeed.
  auto results = finder.FuzzyFind(u"Ta", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Tab 1"));

  results = finder.FuzzyFind(u"Bo", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Bookmarks"));
}

TEST_F(FuzzyFinderTest, TrimsLeadingAndTrailingWhitespace) {
  auto items = CreateItems({{u"New Tab"}, {u"Bookmarks"}});
  FuzzyFinder finder(items);

  // Outer whitespace is stripped so it matches "New Tab"
  auto results = finder.FuzzyFind(u"  New Tab  ", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Tab"));
  EXPECT_LE(results.size(), 3u);

  results = finder.FuzzyFind(u"New Tab ", /*max_results=*/5);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Tab"));
  EXPECT_LE(results.size(), 5u);

  // Whitespace-only query returns empty
  EXPECT_THAT(finder.FuzzyFind(u"   ", /*max_results=*/7), IsEmpty());

  // 1 character padded with spaces (trims to 1 char, < 2) returns empty
  EXPECT_THAT(finder.FuzzyFind(u" t ", /*max_results=*/3), IsEmpty());
}

TEST_F(FuzzyFinderTest, CaseInsensitiveMatching) {
  auto items = CreateItems({{u"Clear Browsing Data"}, {u"PASSWORDS"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"clear", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Clear Browsing Data"));
  EXPECT_LE(results.size(), 3u);

  results = finder.FuzzyFind(u"CLEAR", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Clear Browsing Data"));
  EXPECT_LE(results.size(), 4u);

  results = finder.FuzzyFind(u"passwords", /*max_results=*/5);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"PASSWORDS"));
  EXPECT_LE(results.size(), 5u);

  results = finder.FuzzyFind(u"Passwords", /*max_results=*/6);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"PASSWORDS"));
  EXPECT_LE(results.size(), 6u);
}

TEST_F(FuzzyFinderTest, AccentsAndDiacriticsIgnoring) {
  auto items = CreateItems({{u"Résumé Settings"}, {u"Café Mode"}});
  FuzzyFinder finder(items);

  // Query without accents matches title with accents
  auto results = finder.FuzzyFind(u"resume", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Résumé Settings"));
  EXPECT_LE(results.size(), 1u);

  results = finder.FuzzyFind(u"cafe", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Café Mode"));
  EXPECT_LE(results.size(), 1u);

  // Query with accents matches title without accents
  auto ascii_items = CreateItems({{u"Resume Settings"}});
  FuzzyFinder ascii_finder(ascii_items);
  auto ascii_results = ascii_finder.FuzzyFind(u"résumé", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(ascii_results),
              ElementsAre(u"Resume Settings"));
  EXPECT_LE(ascii_results.size(), 1u);
}

TEST_F(FuzzyFinderTest, UnicodeAndNonLatinStrings) {
  auto items = CreateItems({{u"新しいタブ"}, {u"履歴"}, {u"Настройки"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"新しい", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"新しいタブ"));
  EXPECT_LE(results.size(), 3u);

  results = finder.FuzzyFind(u"настройки", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Настройки"));
  EXPECT_LE(results.size(), 4u);
}

TEST_F(FuzzyFinderTest, MaxResultsZeroReturnsEmpty) {
  auto items = CreateItems({{u"Tab 1"}, {u"Tab 2"}, {u"Tab 3"}});
  FuzzyFinder finder(items);

  // Explicitly requesting 0 results -> returns empty
  EXPECT_THAT(finder.FuzzyFind(u"Tab", /*max_results=*/0), IsEmpty());
}

TEST_F(FuzzyFinderTest, MaxResultsLimitsReturnedCount) {
  auto items =
      CreateItems({{u"Tab 1"}, {u"Tab 2"}, {u"Tab 3"}, {u"Tab 4"}});
  FuzzyFinder finder(items);

  // Capped at custom limit of 2
  auto results = finder.FuzzyFind(u"Tab", /*max_results=*/2);
  EXPECT_EQ(results.size(), 2u);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab 1", u"Tab 2"));
}

TEST_F(FuzzyFinderTest, MaxResultsLessThanTotalMatches) {
  auto items = CreateItems({{u"Tab 1"},
                            {u"Tab 2"},
                            {u"Tab 3"},
                            {u"Tab 4"},
                            {u"Tab 5"},
                            {u"Tab 6"},
                            {u"Tab 7"},
                            {u"Tab 8"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"Tab", /*max_results=*/5);
  EXPECT_EQ(results.size(), 5u);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab 1", u"Tab 2", u"Tab 3", u"Tab 4", u"Tab 5"));
}

TEST_F(FuzzyFinderTest, MaxResultsLargerThanTotalMatches) {
  auto items = CreateItems({{u"Tab 1"}, {u"Tab 2"}, {u"Tab 3"}});
  FuzzyFinder finder(items);

  // Requests 50 results when only 3 match -> safely returns 3
  auto results = finder.FuzzyFind(u"Tab", /*max_results=*/50);
  EXPECT_EQ(results.size(), 3u);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab 1", u"Tab 2", u"Tab 3"));
}

TEST_F(FuzzyFinderTest, EmptyQueryReturnsEmpty) {
  auto items = CreateItems({{u"New Tab"}, {u"History"}});
  FuzzyFinder finder(items);

  EXPECT_THAT(finder.FuzzyFind(u"", /*max_results=*/5), IsEmpty());
}

TEST_F(FuzzyFinderTest, EmptyCorpusReturnsEmpty) {
  FuzzyFinder finder({});

  EXPECT_THAT(finder.FuzzyFind(u"New Tab", /*max_results=*/7), IsEmpty());
}

TEST_F(FuzzyFinderTest, QueryLongerThanTitle) {
  auto items = CreateItems({{u"Tab"}});
  FuzzyFinder finder(items);

  EXPECT_THAT(finder.FuzzyFind(u"Tab Extended Query", /*max_results=*/3),
              IsEmpty());
}

TEST_F(FuzzyFinderTest, SpecialCharactersAndPunctuation) {
  auto items = CreateItems(
      {{u"Settings - Autofill"}, {u"Tab (Grouped)"}, {u"Zoom: 100%"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"- Autofill", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Settings - Autofill"));
  EXPECT_LE(results.size(), 3u);

  results = finder.FuzzyFind(u"(Grouped)", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab (Grouped)"));
  EXPECT_EQ(results.size(), 1u);

  results = finder.FuzzyFind(u"100%", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Zoom: 100%"));
  EXPECT_LE(results.size(), 3u);
}

TEST_F(FuzzyFinderTest, LegacyFindSubstringMatching) {
  auto items = CreateItems({{u"New Tab"}, {u"New Window"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.Find(u"New Tab", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Tab"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].item, items[0]);
}

TEST_F(FuzzyFinderTest, FuzzyFindExactAndWordBoundaryRanking) {
  // Query "tab" (m = 3, max_possible = 80):
  // 1. "Tab" (exact word boundary match):
  //    raw_score = 72, norm = 0.25 + (72/80)*0.75 = 0.9250.
  // 2. "Establish" (infix match, non-boundary):
  //    raw_score = 56, norm = 0.25 + (56/80)*0.75 = 0.7750.
  // 3. "Tag" (typo substitution 'b' -> 'g'):
  //    raw_score = 40, norm = 0.25 + (40/80)*0.75 = 0.6250.
  // 4. "History" (no match): score = 0.0.
  // Expected rank: "Tab" (0.925) > "Establish" (0.775) > "Tag" (0.625).
  auto items = CreateItems({{u"Tab"}, {u"Establish"}, {u"Tag"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"tab", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab", u"Establish", u"Tag"));
  ASSERT_EQ(results.size(), 3u);
  EXPECT_NEAR(results[0].score, 0.925, 0.001);
  EXPECT_NEAR(results[1].score, 0.775, 0.001);
  EXPECT_NEAR(results[2].score, 0.625, 0.001);
  EXPECT_GT(results[0].score, results[1].score);
  EXPECT_GT(results[1].score, results[2].score);
}

TEST_F(FuzzyFinderTest, FuzzyFindAcronymAndMultiWordBonus) {
  // Query "gpm" (m = 3, max_possible = 80):
  // Both "General Performance Monitor" and "Google Password Manager" match
  // 'g', 'p', 'm' at word boundaries with gap penalties between words.
  auto items = CreateItems({{u"Google Password Manager"},
                            {u"General Performance Monitor"},
                            {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"gpm", /*max_results=*/2);
  EXPECT_THAT(
      ExtractResultTitles(results),
      ElementsAre(u"General Performance Monitor", u"Google Password Manager"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, 0.70);
  EXPECT_GT(results[1].score, 0.70);
}

TEST_F(FuzzyFinderTest, FuzzyFindTranspositionSwapTolerance) {
  // Query "abd" (m = 3, max_possible = 80):
  // 1. "adb" (adjacent swap 'b' and 'd'):
  //    raw_score = 32 + 2*16 - 6 = 58, norm = 0.25 + (58/80)*0.75 = 0.79375.
  // 2. "axd" (typo substitution 'b' -> 'x'):
  //    raw_score = 36, norm = 0.25 + (36/80)*0.75 = 0.5875.
  // Expected rank: "adb" (0.7938) > "axd" (0.5875).
  auto items = CreateItems({{u"adb"}, {u"axd"}, {u"xyz"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"abd", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"adb", u"axd"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_NEAR(results[0].score, 0.79375, 0.001);
  EXPECT_NEAR(results[1].score, 0.5875, 0.001);
  EXPECT_GT(results[0].score, results[1].score);
}

TEST_F(FuzzyFinderTest, FuzzyFindTranspositionInitialCharacters) {
  // Transposition at indices 0 and 1: Query "atb" (m = 3, max_possible = 80)
  // matching "Tab" ('t' and 'a' swapped at the beginning):
  // - Row 0: 'a' matches candidate[1] (score 16)
  // - Row 1: 't' matches candidate[0] (is_swap_match, j == 1):
  //          score = (16 * 2) - 6 = 26, consecutive = 2
  // - Row 2: 'b' matches candidate[2]:
  //          score = 26 + 16 + 4 (kConsecutiveBonus) = 46
  // - Normalized score: 0.25 + (46 / 80) * 0.75 = 0.68125.
  auto items = CreateItems({{u"Tab"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"atb", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Tab"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_NEAR(results[0].score, 0.68125, 0.001);
}

TEST_F(FuzzyFinderTest, FuzzyFindTypoTolerance) {
  // Typo: "progile" matching "profile" (g -> f substitution)
  auto items = CreateItems(
      {{u"Customize this profile"}, {u"Close this profile"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"progile", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Customize this profile", u"Close this profile"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, 0.0);
}

TEST_F(FuzzyFinderTest, FuzzyFindTitlePrioritizedOverSynonym) {
  // Query "passkey" (m = 7, max_possible = 176):
  // Both match "passkey" with norm = 0.8977.
  // 1. "Passkey Settings" matches via Title (weight 1.00): final = 0.8977.
  // 2. "Google Password Manager" matches via Synonym (weight 0.85):
  //    final = 0.8977 * 0.85 = 0.7631.
  // Expected rank: "Passkey Settings" > "Google Password Manager".
  auto items = CreateItems({
      {u"Google Password Manager", u"", {u"passkey"}},
      {u"Passkey Settings", u"", {u"credentials"}},
  });
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"passkey", /*max_results=*/2);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Passkey Settings", u"Google Password Manager"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, results[1].score);
}

TEST_F(FuzzyFinderTest, FuzzyFindSynonymOnlyMatch) {
  auto items = CreateItems({
      {u"Google Password Manager", u"", {u"credentials", u"passkey"}},
      {u"Payment methods", u"", {u"credit cards", u"wallet"}},
      {u"History", u"", {u"past pages"}},
  });
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"passkey", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Google Password Manager"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_GT(results[0].score, 0.70);

  results = finder.FuzzyFind(u"wallet", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Payment methods"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_GT(results[0].score, 0.70);
}

TEST_F(FuzzyFinderTest, FuzzyFindStableTieBreakingPreservesOrder) {
  // Identical title match scores should preserve the original corpus order.
  auto items = CreateItems(
      {{u"Bookmark Tab"}, {u"Bookmark All Tabs"}, {u"Bookmark Bar"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"bookmark", /*max_results=*/3);
  EXPECT_THAT(
      ExtractResultTitles(results),
      ElementsAre(u"Bookmark Tab", u"Bookmark All Tabs", u"Bookmark Bar"));
}

TEST_F(FuzzyFinderTest, FuzzyFindQueryLengthAndWhitespaceConstraints) {
  auto items = CreateItems({{u"New Tab"}, {u"Bookmarks"}});
  FuzzyFinder finder(items);

  // Queries under 2 characters or whitespace-only return empty.
  EXPECT_THAT(finder.FuzzyFind(u"", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.FuzzyFind(u"T", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.FuzzyFind(u"   ", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.FuzzyFind(u" t ", /*max_results=*/5), IsEmpty());

  // 2-character query with whitespace trimming succeeds.
  auto results = finder.FuzzyFind(u"  ta  ", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Tab"));
}

TEST_F(FuzzyFinderTest, FuzzyFindMaxResultsCapping) {
  auto items = CreateItems({{u"Tab 1"}, {u"Tab 2"}, {u"Tab 3"}, {u"Tab 4"}});
  FuzzyFinder finder(items);

  EXPECT_THAT(finder.FuzzyFind(u"Tab", /*max_results=*/0), IsEmpty());

  auto results = finder.FuzzyFind(u"Tab", /*max_results=*/2);
  EXPECT_EQ(results.size(), 2u);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Tab 1", u"Tab 2"));
}

TEST_F(FuzzyFinderTest, FuzzyFindCaseAndAccentInsensitive) {
  auto items = CreateItems({{u"Résumé Settings"}, {u"Café Mode"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"resume", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Résumé Settings"));

  results = finder.FuzzyFind(u"CAFE", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Café Mode"));
}

}  // namespace
