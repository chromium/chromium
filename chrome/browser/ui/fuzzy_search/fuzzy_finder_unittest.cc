// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

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
  base::span<const std::u16string> GetSynonyms() const override {
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
  std::vector<raw_ptr<FuzzySearchItem>> CreateItems(
      const std::vector<ItemData>& items_data) {
    std::vector<raw_ptr<FuzzySearchItem>> raw_items;
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

  // Infix / substring match ranks by coverage length:
  // "Brand New" (len 9) > "New Window" (len 10) > "Open New Tab" (len 12)
  results = finder.FuzzyFind(u"New", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Brand New", u"New Window", u"Open New Tab"));
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

TEST_F(FuzzyFinderTest, HasMinQueryLength) {
  // Empty and 1-character queries do not meet the threshold.
  EXPECT_FALSE(FuzzyFinder::HasMinQueryLength(u""));
  EXPECT_FALSE(FuzzyFinder::HasMinQueryLength(u"a"));

  // Two or more characters meet the threshold, including interior whitespace.
  EXPECT_TRUE(FuzzyFinder::HasMinQueryLength(u"ab"));
  EXPECT_TRUE(FuzzyFinder::HasMinQueryLength(u"a b"));

  // Untrimmed queries with leading or trailing whitespace CHECK-fail.
  EXPECT_CHECK_DEATH(FuzzyFinder::HasMinQueryLength(u"   "));
  EXPECT_CHECK_DEATH(FuzzyFinder::HasMinQueryLength(u" a "));
  EXPECT_CHECK_DEATH(FuzzyFinder::HasMinQueryLength(u" ab "));
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

  // 2 characters padded with spaces (trims to 2 chars) succeeds
  results = finder.FuzzyFind(u"  ta  ", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Tab"));
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

// FuzzyFind does not fold accents (see crbug.com/549169077). Accented and
// unaccented text can still match through typo tolerance (substituting the
// accented character) or by aligning the query against unaccented characters
// elsewhere in the candidate. The match ranges below show which characters were
// actually aligned.
TEST_F(FuzzyFinderTest, AccentedTextMatchesViaTypoTolerance) {
  auto items = CreateItems({{u"Résumé Settings"}, {u"Café Mode"}});
  FuzzyFinder finder(items);

  // The trailing 'e' of "resume" aligns with the 'e' in "Settings" rather than
  // with 'é'.
  auto results = finder.FuzzyFind(u"resume", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Résumé Settings"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(
      results[0].match_ranges,
      ElementsAre(gfx::Range(0, 1), gfx::Range(2, 5), gfx::Range(8, 9)));

  // The trailing 'e' of "cafe" aligns with the 'e' in "Mode" rather than with
  // 'é'.
  results = finder.FuzzyFind(u"cafe", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Café Mode"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0].match_ranges,
              ElementsAre(gfx::Range(0, 3), gfx::Range(8, 9)));

  results = finder.FuzzyFind(u"CAFE", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Café Mode"));

  // An accented query matches unaccented text by substituting 'é' for 'e',
  // which is not an exact match and so is not highlighted.
  auto ascii_items = CreateItems({{u"Cafe Mode"}});
  FuzzyFinder ascii_finder(ascii_items);
  auto ascii_results = ascii_finder.FuzzyFind(u"café", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(ascii_results), ElementsAre(u"Cafe Mode"));
  ASSERT_EQ(ascii_results.size(), 1u);
  EXPECT_THAT(ascii_results[0].match_ranges, ElementsAre(gfx::Range(0, 3)));

  // Queries of 3 or fewer characters do not allow substitutions, so "ete"
  // cannot match "Été", which has no unaccented 'e' to align with.
  // TODO(crbug.com/549169077): This should match once accents are folded.
  auto short_items = CreateItems({{u"Été"}});
  FuzzyFinder short_finder(short_items);
  EXPECT_THAT(short_finder.FuzzyFind(u"ete", /*max_results=*/1), IsEmpty());
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
  EXPECT_THAT(results[0].match_ranges, ElementsAre(gfx::Range(0, 7)));
  EXPECT_EQ(results[0].exact_match_range, gfx::Range(0, 7));
}

TEST_F(FuzzyFinderTest, FuzzyFindExactAndWordBoundaryRanking) {
  // Query "tab" (m = 3, max_possible = 80):
  // 1. "Tab" (exact word boundary match, n = 3):
  //    raw_score = 76, alignment_norm = 0.25 + (76/80)*0.73 = 0.9435
  //    coverage_bonus = (3/3)*0.02 = 0.02
  //    total = 0.9635.
  // 2. "Establish" (infix match, non-boundary, n = 9):
  //    raw_score = 60, alignment_norm = 0.25 + (60/80)*0.73 = 0.7975
  //    coverage_bonus = (3/9)*0.02 = 0.0066
  //    total = 0.8041.
  // 3. "Tag" (typo substitution 'b' -> 'g'): rejected for short queries (<= 3).
  // 4. "History" (no match): score = 0.0.
  // Expected rank: "Tab" (0.9635) > "Establish" (0.8041).
  auto items = CreateItems({{u"Tab"}, {u"Establish"}, {u"Tag"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"tab", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Tab", u"Establish"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_NEAR(results[0].score, 0.9635, 0.001);
  EXPECT_NEAR(results[1].score, 0.8041, 0.001);
  EXPECT_GT(results[0].score, results[1].score);
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
      ElementsAre(u"Google Password Manager", u"General Performance Monitor"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, 0.55);
  EXPECT_GT(results[1].score, 0.55);
}

TEST_F(FuzzyFinderTest, FuzzyFindTranspositionSwapTolerance) {
  // Query "abcd" (m = 4, max_possible = 104):
  // 1. "acbd" (adjacent swap 'b' and 'c'):
  //    raw_score = 32 + (2*16 - 6) + (16 + 6) = 80,
  //    norm = 0.25 + (80/104)*0.73 = 0.8115.
  //    coverage = 0.02 * 4/4 = 0.02, total = 0.8315.
  // 2. "axcd" (typo substitution 'b' -> 'x', allowed for m > 3):
  //    raw_score = 32 + (-12) + 16 + (16 + 6) = 58,
  //    norm = 0.25 + (58/104)*0.73 = 0.6571.
  //    coverage = 0.02 * 4/4 = 0.02, total = 0.6771.
  // Expected rank: "acbd" (0.8315) > "axcd" (0.6771).
  auto items = CreateItems({{u"acbd"}, {u"axcd"}, {u"xyzw"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"abcd", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"acbd", u"axcd"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_NEAR(results[0].score, 0.8315, 0.001);
  EXPECT_NEAR(results[1].score, 0.6771, 0.001);
  EXPECT_GT(results[0].score, results[1].score);
}

TEST_F(FuzzyFinderTest, FuzzyFindTranspositionInitialCharacters) {
  // Transposition at indices 0 and 1: Query "atb" (m = 3, max_possible = 80)
  // matching "Tab" ('t' and 'a' swapped at the beginning):
  // - Row 0: 'a' matches candidate[1] (score 16)
  // - Row 1: 't' matches candidate[0] (is_swap_match, j == 1):
  //          score = (16 * 2) - 6 = 26, consecutive = 2
  // - Row 2: 'b' matches candidate[2]:
  //          score = 26 + 16 + 6 (kConsecutiveBonus) = 48
  // - Normalized score: 0.25 + (48 / 80) * 0.73 = 0.688.
  // - Coverage: 0.02 * 3/3 = 0.02, total = 0.708.
  auto items = CreateItems({{u"Tab"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"atb", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Tab"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_NEAR(results[0].score, 0.708, 0.001);
}

TEST_F(FuzzyFinderTest, FuzzyFindTypoTolerance) {
  // Typo: "progile" matching "profile" (g -> f substitution)
  auto items = CreateItems(
      {{u"Customize this profile"}, {u"Close this profile"}, {u"History"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"progile", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Close this profile", u"Customize this profile"));
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
  // Identical title match scores and same candidate length should preserve the
  // original corpus order.
  auto items = CreateItems({{u"Bookmark Bar"}, {u"Bookmark Tab"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"bookmark", /*max_results=*/2);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Bookmark Bar", u"Bookmark Tab"));
}

TEST_F(FuzzyFinderTest, FuzzyFindSecondaryTextMatch) {
  // Query matches secondary text when title does not match.
  auto items = CreateItems({
      {u"Google Password Manager", u"Passwords and autofill", {u"credentials"}},
      {u"History", u"Past browsing data", {}},
  });
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"autofill", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Google Password Manager"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_GT(results[0].score, 0.70);
}

TEST_F(FuzzyFinderTest, FuzzyFindTitlePrioritizedOverSecondaryText) {
  // Both match "autofill":
  // 1. "Autofill Settings" matches via Title (weight 1.00).
  // 2. "Google Password Manager" matches via Secondary Text (weight 0.80).
  // Expected rank: "Autofill Settings" > "Google Password Manager".
  auto items = CreateItems({
      {u"Google Password Manager", u"Autofill Settings"},
      {u"Autofill Settings", u"General"},
  });
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"autofill", /*max_results=*/2);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Autofill Settings", u"Google Password Manager"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, results[1].score);
}

TEST_F(FuzzyFinderTest, FuzzyFindSynonymPrioritizedOverSecondaryText) {
  // Both match "credentials":
  // 1. "Password Manager" matches via Synonym (weight 0.85).
  // 2. "Passkey Settings" matches via Secondary Text (weight 0.80).
  // Expected rank: "Password Manager" > "Passkey Settings".
  auto items = CreateItems({
      {u"Passkey Settings", u"credentials"},
      {u"Password Manager", u"General", {u"credentials"}},
  });
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"credentials", /*max_results=*/2);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Password Manager", u"Passkey Settings"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, results[1].score);
}

TEST_F(FuzzyFinderTest, ContiguousDominanceOverScatteredSubsequences) {
  // Contiguous match ("Performance Monitor") vs scattered subsequence match
  // ("Page Event Resource Filter") for query "perf".
  auto items =
      CreateItems({{u"Page Event Resource Filter"}, {u"Performance Monitor"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"perf", /*max_results=*/2);
  ASSERT_EQ(results.size(), 2u);
  // Contiguous prefix match must rank first and heavily outscore scattered
  // match.
  EXPECT_EQ(results[0].item->GetTitle(), u"Performance Monitor");
  EXPECT_EQ(results[1].item->GetTitle(), u"Page Event Resource Filter");
  EXPECT_GT(results[0].score, 0.90);
  EXPECT_GT(results[0].score - results[1].score, 0.25);
}

TEST_F(FuzzyFinderTest, ShortQueryRejectsSingleCharacterSubstitutions) {
  // For queries with length <= 3, arbitrary substitutions (typos) are
  // disallowed.
  auto items = CreateItems({{u"Tab"}, {u"Tag"}, {u"Tax"}, {u"Tad"}});
  FuzzyFinder finder(items);

  // 3-character query "tab" must match "Tab" exactly and reject "Tag", "Tax",
  // "Tad".
  auto results = finder.FuzzyFind(u"tab", /*max_results=*/5);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Tab"));
  EXPECT_EQ(results.size(), 1u);

  // 2-character query "ta" must reject single-character substitution "to".
  auto items2 = CreateItems({{u"Ta"}, {u"To"}});
  FuzzyFinder finder2(items2);
  auto results2 = finder2.FuzzyFind(u"ta", /*max_results=*/5);
  EXPECT_THAT(ExtractResultTitles(results2), ElementsAre(u"Ta"));
}

TEST_F(FuzzyFinderTest, MinScoreCutoffFiltersWeakMatches) {
  // Items with varying match quality for query "perf":
  // - "Performance Monitor": Contiguous prefix match (~0.96 >= 0.60)
  // - "Page Event Resource Filter": Word boundary scattered match (~0.67 >=
  // 0.60)
  // - "Operation": Low-quality match with non-boundary start and typo (< 0.60)
  auto items = CreateItems({
      {u"Performance Monitor"},
      {u"Page Event Resource Filter"},
      {u"Operation"},
  });
  FuzzyFinder finder(items);

  // Default internal cutoff (0.60) keeps the high-quality and scattered matches
  // while filtering out weak matches below 0.60.
  auto results = finder.FuzzyFind(u"perf", /*max_results=*/5);
  EXPECT_THAT(
      ExtractResultTitles(results),
      ElementsAre(u"Performance Monitor", u"Page Event Resource Filter"));
}

TEST_F(FuzzyFinderTest, MatchRangesComputation) {
  // 1. Prefix match: "New Tab" with query "New" -> [0, 3)
  auto prefix_items = CreateItems({{u"New Tab"}});
  FuzzyFinder prefix_finder(prefix_items);
  auto prefix_results = prefix_finder.FuzzyFind(u"New", /*max_results=*/1);
  ASSERT_EQ(prefix_results.size(), 1u);
  EXPECT_THAT(prefix_results[0].match_ranges, ElementsAre(gfx::Range(0, 3)));
  EXPECT_EQ(prefix_results[0].exact_match_range, gfx::Range(0, 3));

  // 2. Infix and inner-word matches: "Open New Tab" with query "New" -> [5, 8),
  // and "New Incognito Window" with query "cognito" -> [6, 13).
  auto infix_items = CreateItems({{u"Open New Tab"}});
  FuzzyFinder infix_finder(infix_items);
  auto infix_results = infix_finder.FuzzyFind(u"New", /*max_results=*/1);
  ASSERT_EQ(infix_results.size(), 1u);
  EXPECT_THAT(infix_results[0].match_ranges, ElementsAre(gfx::Range(5, 8)));
  EXPECT_EQ(infix_results[0].exact_match_range, gfx::Range(5, 8));

  auto inner_items = CreateItems({{u"New Incognito Window"}});
  FuzzyFinder inner_finder(inner_items);
  auto inner_results = inner_finder.FuzzyFind(u"cognito", /*max_results=*/1);
  ASSERT_EQ(inner_results.size(), 1u);
  EXPECT_THAT(inner_results[0].match_ranges, ElementsAre(gfx::Range(6, 13)));
  EXPECT_EQ(inner_results[0].exact_match_range, gfx::Range(6, 13));

  // 3. Accent-insensitive exact match: "Café Mode" with query "cafe" populates
  // `exact_match_range` for the contiguous word while `match_ranges` reflects
  // the unaccented fuzzy character alignment.
  auto accent_items = CreateItems({{u"Café Mode"}});
  FuzzyFinder accent_finder(accent_items);
  auto accent_results = accent_finder.FuzzyFind(u"cafe", /*max_results=*/1);
  ASSERT_EQ(accent_results.size(), 1u);
  EXPECT_THAT(accent_results[0].match_ranges,
              ElementsAre(gfx::Range(0, 3), gfx::Range(8, 9)));
  EXPECT_EQ(accent_results[0].exact_match_range, gfx::Range(0, 4));

  // 4. Transposition match: "Tab" with query "atb" merges into a single fuzzy
  // range [0, 3) but is not an exact match, so `exact_match_range` is invalid.
  auto swap_items = CreateItems({{u"Tab"}});
  FuzzyFinder swap_finder(swap_items);
  auto swap_results = swap_finder.FuzzyFind(u"atb", /*max_results=*/1);
  ASSERT_EQ(swap_results.size(), 1u);
  EXPECT_THAT(swap_results[0].match_ranges, ElementsAre(gfx::Range(0, 3)));
  EXPECT_EQ(swap_results[0].exact_match_range, gfx::Range::InvalidRange());

  // 5. Multi-word acronym match: "Google Chrome Browser" with query "gcb"
  auto multi_items = CreateItems({{u"Google Chrome Browser"}});
  FuzzyFinder multi_finder(multi_items);
  auto multi_results = multi_finder.FuzzyFind(u"gcb", /*max_results=*/1);
  ASSERT_EQ(multi_results.size(), 1u);
  EXPECT_THAT(
      multi_results[0].match_ranges,
      ElementsAre(gfx::Range(0, 1), gfx::Range(7, 8), gfx::Range(14, 15)));
  EXPECT_EQ(multi_results[0].exact_match_range, gfx::Range::InvalidRange());

  // 6. Synonym match leaves match_ranges empty and exact_match_range invalid.
  auto syn_items = CreateItems({{u"Settings", u"", {u"Preferences"}}});
  FuzzyFinder syn_finder(syn_items);
  auto syn_results = syn_finder.FuzzyFind(u"Preferences", /*max_results=*/1);
  ASSERT_EQ(syn_results.size(), 1u);
  EXPECT_TRUE(syn_results[0].match_ranges.empty());
  EXPECT_EQ(syn_results[0].exact_match_range, gfx::Range::InvalidRange());

  // 7. Secondary text match leaves match_ranges empty and exact_match_range
  // invalid.
  auto sec_items = CreateItems({{u"Settings", u"Preferences"}});
  FuzzyFinder sec_finder(sec_items);
  auto sec_results = sec_finder.FuzzyFind(u"Preferences", /*max_results=*/1);
  ASSERT_EQ(sec_results.size(), 1u);
  EXPECT_TRUE(sec_results[0].match_ranges.empty());
  EXPECT_EQ(sec_results[0].exact_match_range, gfx::Range::InvalidRange());
}

TEST_F(FuzzyFinderTest, DefaultWeightsMatchSingleArgConstructor) {
  auto items = CreateItems({
      {u"Google Password Manager", u"", {u"passkey"}},
      {u"Passkey Settings", u"credentials"},
  });
  FuzzyFinder default_finder(items);
  FuzzyFinder weighted_finder(items, FuzzyFinder::ScoringParams());

  auto default_results = default_finder.FuzzyFind(u"passkey", 2);
  auto weighted_results = weighted_finder.FuzzyFind(u"passkey", 2);
  EXPECT_EQ(ExtractResultTitles(default_results),
            ExtractResultTitles(weighted_results));
  ASSERT_EQ(default_results.size(), weighted_results.size());
  for (size_t i = 0; i < default_results.size(); ++i) {
    EXPECT_DOUBLE_EQ(default_results[i].score, weighted_results[i].score);
  }
}

TEST_F(FuzzyFinderTest, CustomWeightsReorderTitleAndSynonymMatches) {
  // Both items match "passkey" equally well (norm = 0.9489), one via its title
  // and one via a synonym, so the ranking is decided by the field weights.
  auto items = CreateItems({
      {u"Google Password Manager", u"", {u"passkey"}},
      {u"Passkey Settings", u"", {u"credentials"}},
  });

  // Default weights (title 1.00 > synonym 0.85) rank the title match first.
  FuzzyFinder default_finder(items);
  EXPECT_THAT(ExtractResultTitles(default_finder.FuzzyFind(u"passkey", 2)),
              ElementsAre(u"Passkey Settings", u"Google Password Manager"));

  // Weighting synonyms above titles flips the order:
  // synonym 0.9489 * 1.00 = 0.9489 > title 0.9489 * 0.80 = 0.7591.
  FuzzyFinder synonym_heavy_finder(
      items, {.title = 0.80, .secondary_text = 0.80, .synonym = 1.00});
  auto results = synonym_heavy_finder.FuzzyFind(u"passkey", 2);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Google Password Manager", u"Passkey Settings"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, results[1].score);
}

TEST_F(FuzzyFinderTest, CustomWeightsReorderSynonymAndSecondaryTextMatches) {
  // Both items match "credentials", one via secondary text and one via a
  // synonym, so the ranking is decided by the field weights.
  auto items = CreateItems({
      {u"Passkey Settings", u"credentials"},
      {u"Password Manager", u"General", {u"credentials"}},
  });

  // Default weights (synonym 0.85 > secondary 0.80) rank the synonym first.
  FuzzyFinder default_finder(items);
  EXPECT_THAT(ExtractResultTitles(default_finder.FuzzyFind(u"credentials", 2)),
              ElementsAre(u"Password Manager", u"Passkey Settings"));

  // Weighting secondary text above synonyms flips the order.
  FuzzyFinder secondary_heavy_finder(
      items, {.title = 1.00, .secondary_text = 0.90, .synonym = 0.75});
  auto results = secondary_heavy_finder.FuzzyFind(u"credentials", 2);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Passkey Settings", u"Password Manager"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, results[1].score);
}

TEST_F(FuzzyFinderTest, LowWeightDropsMatchesBelowMinScore) {
  // A synonym-only match (norm = 0.9489) survives the 0.60 cutoff with the
  // default synonym weight, but not when the weight pushes it below 0.60.
  auto items = CreateItems({
      {u"Google Password Manager", u"", {u"passkey"}},
      {u"Passkey Settings"},
  });

  FuzzyFinder default_finder(items);
  EXPECT_THAT(ExtractResultTitles(default_finder.FuzzyFind(u"passkey", 2)),
              ElementsAre(u"Passkey Settings", u"Google Password Manager"));

  // 0.9489 * 0.50 = 0.4745 < 0.60.
  FuzzyFinder low_synonym_finder(
      items, {.title = 1.00, .secondary_text = 0.80, .synonym = 0.50});
  EXPECT_THAT(ExtractResultTitles(low_synonym_finder.FuzzyFind(u"passkey", 2)),
              ElementsAre(u"Passkey Settings"));
}

TEST_F(FuzzyFinderTest, WeightsAreClampedToUnitRange) {
  auto items = CreateItems({{u"Passkey Settings"}});

  // Weights above 1 are clamped to 1, so scores never exceed the default
  // title-weight score.
  FuzzyFinder default_finder(items);
  FuzzyFinder high_weight_finder(
      items, {.title = 5.0, .secondary_text = 5.0, .synonym = 5.0});
  auto default_results = default_finder.FuzzyFind(u"passkey", 1);
  auto high_results = high_weight_finder.FuzzyFind(u"passkey", 1);
  ASSERT_EQ(default_results.size(), 1u);
  ASSERT_EQ(high_results.size(), 1u);
  EXPECT_DOUBLE_EQ(high_results[0].score, default_results[0].score);
  EXPECT_LE(high_results[0].score, 1.0);

  // Negative weights are clamped to 0, which drops the match entirely.
  FuzzyFinder negative_weight_finder(
      items, {.title = -1.0, .secondary_text = -1.0, .synonym = -1.0});
  EXPECT_THAT(negative_weight_finder.FuzzyFind(u"passkey", 1), IsEmpty());
}

TEST_F(FuzzyFinderTest, MinScoreParamChangesCutoff) {
  // Same corpus as MinScoreCutoffFiltersWeakMatches, for query "perf":
  // - "Performance Monitor": ~0.96
  // - "Page Event Resource Filter": ~0.67
  // - "Operation": weak match below 0.60
  // - "History": no match (score 0)
  auto items = CreateItems({
      {u"Performance Monitor"},
      {u"Page Event Resource Filter"},
      {u"Operation"},
      {u"History"},
  });

  // Default cutoff (0.60).
  FuzzyFinder default_finder(items);
  EXPECT_THAT(
      ExtractResultTitles(default_finder.FuzzyFind(u"perf", 5)),
      ElementsAre(u"Performance Monitor", u"Page Event Resource Filter"));

  // A stricter cutoff drops the scattered match.
  FuzzyFinder strict_finder(items, {.min_score = 0.80});
  EXPECT_THAT(ExtractResultTitles(strict_finder.FuzzyFind(u"perf", 5)),
              ElementsAre(u"Performance Monitor"));

  // A cutoff of 0 keeps every match, but never returns non-matches.
  FuzzyFinder lenient_finder(items, {.min_score = 0.0});
  auto lenient_titles =
      ExtractResultTitles(lenient_finder.FuzzyFind(u"perf", 5));
  EXPECT_THAT(lenient_titles, ::testing::Contains(u"Performance Monitor"));
  EXPECT_THAT(lenient_titles,
              ::testing::Contains(u"Page Event Resource Filter"));
  EXPECT_THAT(lenient_titles, ::testing::Not(::testing::Contains(u"History")));
}

TEST_F(FuzzyFinderTest, CoverageBonusRanksExactMatchFirst) {
  auto items = CreateItems({{u"Import bookmarks and settings"}, {u"Settings"}});
  FuzzyFinder finder(items);

  auto results = finder.FuzzyFind(u"settings", /*max_results=*/2);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Settings", u"Import bookmarks and settings"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_GT(results[0].score, results[1].score);
}

}  // namespace
