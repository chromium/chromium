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

  auto results = finder.Find(u"New Tab", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Tab"));
  EXPECT_LE(results.size(), 3u);
  EXPECT_EQ(results[0].item, items[0]);
}

TEST_F(FuzzyFinderTest, SubstringPrefixAndSuffixMatches) {
  auto items =
      CreateItems({{u"Open New Tab"}, {u"New Window"}, {u"Brand New"}});
  FuzzyFinder finder(items);

  // Prefix match
  auto results = finder.Find(u"Open", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Open New Tab"));
  EXPECT_LE(results.size(), 4u);

  // Infix / substring match
  results = finder.Find(u"New", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Open New Tab", u"New Window", u"Brand New"));
  EXPECT_LE(results.size(), 3u);

  // Suffix match
  results = finder.Find(u"Window", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Window"));
  EXPECT_LE(results.size(), 1u);
}

TEST_F(FuzzyFinderTest, NoMatchReturnsEmpty) {
  auto items = CreateItems({{u"Bookmarks"}, {u"Downloads"}, {u"History"}});
  FuzzyFinder finder(items);


  EXPECT_THAT(finder.Find(u"Settings", /*max_results=*/5), IsEmpty());
}

TEST_F(FuzzyFinderTest, QueryShorterThanThreeCharactersReturnsEmpty) {
  auto items = CreateItems({{u"Tab 1"}, {u"History"}});
  FuzzyFinder finder(items);

  // 1-character and 2-character queries return empty per design doc step 1
  EXPECT_THAT(finder.Find(u"T", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.Find(u"Ta", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.Find(u"Hi", /*max_results=*/5), IsEmpty());
  EXPECT_THAT(finder.Find(u"ai", /*max_results=*/5), IsEmpty());

  // 3-character queries succeed
  auto results = finder.Find(u"Tab", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Tab 1"));
  EXPECT_LE(results.size(), 3u);
}

TEST_F(FuzzyFinderTest, TrimsLeadingAndTrailingWhitespace) {
  auto items = CreateItems({{u"New Tab"}, {u"History"}});
  FuzzyFinder finder(items);

  // Outer whitespace is stripped so it matches "New Tab"
  auto results = finder.Find(u"  New Tab  ", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"New Tab"));
  EXPECT_LE(results.size(), 3u);

  results = finder.Find(u"New Tab ", /*max_results=*/5);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"New Tab"));
  EXPECT_LE(results.size(), 5u);

  // Whitespace-only query returns empty
  EXPECT_THAT(finder.Find(u"   ", /*max_results=*/7), IsEmpty());

  // 2 characters padded with spaces (trims to 2 chars, < 3) returns empty
  EXPECT_THAT(finder.Find(u" ta ", /*max_results=*/3), IsEmpty());
}

TEST_F(FuzzyFinderTest, CaseInsensitiveMatching) {
  auto items = CreateItems({{u"Clear Browsing Data"}, {u"PASSWORDS"}});
  FuzzyFinder finder(items);

  auto results = finder.Find(u"clear", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Clear Browsing Data"));
  EXPECT_LE(results.size(), 3u);

  results = finder.Find(u"CLEAR", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Clear Browsing Data"));
  EXPECT_LE(results.size(), 4u);

  results = finder.Find(u"passwords", /*max_results=*/5);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"PASSWORDS"));
  EXPECT_LE(results.size(), 5u);

  results = finder.Find(u"Passwords", /*max_results=*/6);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"PASSWORDS"));
  EXPECT_LE(results.size(), 6u);
}

TEST_F(FuzzyFinderTest, AccentsAndDiacriticsIgnoring) {
  auto items = CreateItems({{u"Résumé Settings"}, {u"Café Mode"}});
  FuzzyFinder finder(items);

  // Query without accents matches title with accents
  auto results = finder.Find(u"resume", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results), ElementsAre(u"Résumé Settings"));
  EXPECT_LE(results.size(), 3u);

  results = finder.Find(u"cafe", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Café Mode"));
  EXPECT_LE(results.size(), 1u);

  // Query with accents matches title without accents
  auto ascii_items = CreateItems({{u"Resume Settings"}});
  FuzzyFinder ascii_finder(ascii_items);
  auto ascii_results = ascii_finder.Find(u"résumé", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(ascii_results),
              ElementsAre(u"Resume Settings"));
  EXPECT_LE(ascii_results.size(), 3u);
}

TEST_F(FuzzyFinderTest, UnicodeAndNonLatinStrings) {
  auto items = CreateItems({{u"新しいタブ"}, {u"履歴"}, {u"Настройки"}});
  FuzzyFinder finder(items);

  auto results = finder.Find(u"新しい", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"新しいタブ"));
  EXPECT_LE(results.size(), 3u);

  results = finder.Find(u"настройки", /*max_results=*/4);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Настройки"));
  EXPECT_LE(results.size(), 4u);
}

TEST_F(FuzzyFinderTest, MaxResultsZeroReturnsEmpty) {
  auto items = CreateItems({{u"Tab 1"}, {u"Tab 2"}, {u"Tab 3"}});
  FuzzyFinder finder(items);

  // Explicitly requesting 0 results -> returns empty
  EXPECT_THAT(finder.Find(u"Tab", /*max_results=*/0), IsEmpty());
}

TEST_F(FuzzyFinderTest, MaxResultsLimitsReturnedCount) {
  auto items =
      CreateItems({{u"Tab 1"}, {u"Tab 2"}, {u"Tab 3"}, {u"Tab 4"}});
  FuzzyFinder finder(items);

  // Capped at custom limit of 2
  auto results = finder.Find(u"Tab", /*max_results=*/2);
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

  auto results = finder.Find(u"Tab", /*max_results=*/5);
  EXPECT_EQ(results.size(), 5u);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab 1", u"Tab 2", u"Tab 3", u"Tab 4", u"Tab 5"));
}

TEST_F(FuzzyFinderTest, MaxResultsLargerThanTotalMatches) {
  auto items = CreateItems({{u"Tab 1"}, {u"Tab 2"}, {u"Tab 3"}});
  FuzzyFinder finder(items);

  // Requests 50 results when only 3 match -> safely returns 3
  auto results = finder.Find(u"Tab", /*max_results=*/50);
  EXPECT_EQ(results.size(), 3u);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab 1", u"Tab 2", u"Tab 3"));
}

TEST_F(FuzzyFinderTest, EmptyQueryReturnsEmpty) {
  auto items = CreateItems({{u"New Tab"}, {u"History"}});
  FuzzyFinder finder(items);

  EXPECT_THAT(finder.Find(u"", /*max_results=*/5), IsEmpty());
}

TEST_F(FuzzyFinderTest, EmptyCorpusReturnsEmpty) {
  FuzzyFinder finder({});

  EXPECT_THAT(finder.Find(u"New Tab", /*max_results=*/7), IsEmpty());
}

TEST_F(FuzzyFinderTest, QueryLongerThanTitle) {
  auto items = CreateItems({{u"Tab"}});
  FuzzyFinder finder(items);

  EXPECT_THAT(finder.Find(u"Tab Extended Query", /*max_results=*/3), IsEmpty());
}

TEST_F(FuzzyFinderTest, SpecialCharactersAndPunctuation) {
  auto items = CreateItems(
      {{u"Settings - Autofill"}, {u"Tab (Grouped)"}, {u"Zoom: 100%"}});
  FuzzyFinder finder(items);

  auto results = finder.Find(u"- Autofill", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Settings - Autofill"));
  EXPECT_LE(results.size(), 3u);

  results = finder.Find(u"(Grouped)", /*max_results=*/1);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Tab (Grouped)"));
  EXPECT_EQ(results.size(), 1u);

  results = finder.Find(u"100%", /*max_results=*/3);
  EXPECT_THAT(ExtractResultTitles(results),
              ElementsAre(u"Zoom: 100%"));
  EXPECT_LE(results.size(), 3u);
}

}  // namespace
