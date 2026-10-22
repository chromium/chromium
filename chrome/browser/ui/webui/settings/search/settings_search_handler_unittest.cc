// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_search_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/mojom/settings_search.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

namespace settings_search {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kAppearanceRoute[] = "/appearance";
constexpr char kFontsRoute[] = "/fonts";
constexpr size_t kMaxQueryLength = SettingsSearchHandler::kMaxQueryLength;

std::unique_ptr<SettingsSearchItem> MakeItem(
    mojom::SettingId id,
    const std::string& route,
    const std::u16string& title,
    std::vector<std::u16string> breadcrumbs = {u"Appearance"},
    std::vector<std::u16string> synonyms = {}) {
  return std::make_unique<SettingsSearchItem>(
      id, route, title, std::move(breadcrumbs), std::move(synonyms));
}

std::vector<std::unique_ptr<SettingsSearchItem>> MakeAppearanceItems() {
  std::vector<std::unique_ptr<SettingsSearchItem>> items;
  items.push_back(MakeItem(mojom::SettingId::kShowHomeButton, kAppearanceRoute,
                           u"Show home button", {u"Appearance"},
                           {u"homepage"}));
  items.push_back(MakeItem(mojom::SettingId::kShowBookmarksBar,
                           kAppearanceRoute, u"Show bookmarks bar"));
  items.push_back(MakeItem(mojom::SettingId::kFontSize, kAppearanceRoute,
                           u"Font size", {u"Appearance"}, {u"text size"}));
  items.push_back(MakeItem(mojom::SettingId::kCustomizeFonts, kFontsRoute,
                           u"Customize fonts"));
  items.push_back(
      MakeItem(mojom::SettingId::kPageZoom, kAppearanceRoute, u"Page zoom"));
  return items;
}

class SettingsSearchHandlerTest : public testing::Test {
 protected:
  void CreateHandler(std::vector<std::unique_ptr<SettingsSearchItem>> items) {
    handler_ = std::make_unique<SettingsSearchHandler>(
        remote_.BindNewPipeAndPassReceiver(), std::move(items));
  }

  void CreateHandlerWithTitle(const std::u16string& title) {
    std::vector<std::unique_ptr<SettingsSearchItem>> items;
    items.push_back(
        MakeItem(mojom::SettingId::kPageZoom, kAppearanceRoute, title));
    CreateHandler(std::move(items));
  }

  std::vector<mojom::SearchResultPtr> Search(
      const std::u16string& query,
      uint32_t max_results = SettingsSearchHandler::kMaxResults) {
    base::test::TestFuture<std::vector<mojom::SearchResultPtr>> future;
    remote_->Search(query, max_results, future.GetCallback());
    return future.Take();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      {features::kWebuiRefresh2026, features::kSettingsRefresh2026,
       features::kSettingsSearch2026},
      /*disabled_features=*/{}};
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::SettingsSearchHandler> remote_;
  std::unique_ptr<SettingsSearchHandler> handler_;
};

TEST_F(SettingsSearchHandlerTest, TitleMatchPopulatesAllFields) {
  CreateHandler(MakeAppearanceItems());

  auto results = Search(u"bookmarks bar");

  ASSERT_EQ(results.size(), 1u);
  const mojom::SearchResult& result = *results[0];
  EXPECT_EQ(result.id, mojom::SettingId::kShowBookmarksBar);
  EXPECT_EQ(result.route, kAppearanceRoute);
  EXPECT_EQ(result.title, u"Show bookmarks bar");
  EXPECT_THAT(result.breadcrumbs, ElementsAre(u"Appearance"));
  EXPECT_GT(result.score, 0.0);
  EXPECT_LE(result.score, 1.0);
  EXPECT_THAT(result.title_match_ranges, ElementsAre(gfx::Range(5, 18)));
}

TEST_F(SettingsSearchHandlerTest, SynonymMatchHasNoTitleRanges) {
  CreateHandler(MakeAppearanceItems());

  auto results = Search(u"homepage");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0]->id, mojom::SettingId::kShowHomeButton);
  EXPECT_EQ(results[0]->title, u"Show home button");
  EXPECT_THAT(results[0]->title_match_ranges, IsEmpty());
}

TEST_F(SettingsSearchHandlerTest, BreadcrumbMatchHasNoTitleRanges) {
  CreateHandlerWithTitle(u"Page zoom");

  auto results = Search(u"appearance");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0]->breadcrumbs, ElementsAre(u"Appearance"));
  EXPECT_THAT(results[0]->title_match_ranges, IsEmpty());
}

TEST_F(SettingsSearchHandlerTest, OnlyInnermostBreadcrumbIsSearched) {
  std::vector<std::unique_ptr<SettingsSearchItem>> items;
  items.push_back(MakeItem(mojom::SettingId::kCustomizeFonts, kFontsRoute,
                           u"Standard font",
                           {u"Appearance", u"Customize fonts"}));
  CreateHandler(std::move(items));

  auto results = Search(u"customize");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0]->breadcrumbs,
              ElementsAre(u"Appearance", u"Customize fonts"));
  EXPECT_THAT(Search(u"appearance"), IsEmpty());
}

TEST_F(SettingsSearchHandlerTest, ItemWithoutBreadcrumbs) {
  std::vector<std::unique_ptr<SettingsSearchItem>> items;
  items.push_back(MakeItem(mojom::SettingId::kAppearance, kAppearanceRoute,
                           u"Appearance", /*breadcrumbs=*/{}));
  CreateHandler(std::move(items));

  auto results = Search(u"appearance");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0]->breadcrumbs, IsEmpty());
}

TEST_F(SettingsSearchHandlerTest, LimitsResultCount) {
  std::vector<std::unique_ptr<SettingsSearchItem>> items;
  for (size_t i = 0; i < SettingsSearchHandler::kMaxResults + 1; ++i) {
    items.push_back(MakeItem(mojom::SettingId::kAppearance, kAppearanceRoute,
                             u"Appearance"));
  }
  CreateHandler(std::move(items));

  EXPECT_EQ(Search(u"appearance", /*max_results=*/2).size(), 2u);
  EXPECT_EQ(Search(u"appearance", /*max_results=*/1000).size(),
            SettingsSearchHandler::kMaxResults);
  EXPECT_THAT(Search(u"appearance", /*max_results=*/0), IsEmpty());
}

TEST_F(SettingsSearchHandlerTest, ShortOrBlankQueryReturnsNothing) {
  CreateHandler(MakeAppearanceItems());
  EXPECT_THAT(Search(u""), IsEmpty());
  EXPECT_THAT(Search(u"   "), IsEmpty());
  EXPECT_THAT(Search(u"f"), IsEmpty());
}

TEST_F(SettingsSearchHandlerTest, TruncatesOverlongQuery) {
  const std::u16string title(kMaxQueryLength, u'a');
  CreateHandlerWithTitle(title);

  auto results = Search(title + u"bcd");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0]->title_match_ranges,
              ElementsAre(gfx::Range(0, kMaxQueryLength)));
}

TEST_F(SettingsSearchHandlerTest, TrimsWhitespaceBeforeTruncating) {
  const std::u16string title(kMaxQueryLength, u'a');
  CreateHandlerWithTitle(title);

  auto results = Search(u" \t" + title + u"\n");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0]->title_match_ranges,
              ElementsAre(gfx::Range(0, kMaxQueryLength)));
}

TEST_F(SettingsSearchHandlerTest, TruncationKeepsSurrogatePairAtLimit) {
  // The emoji's two code units are the last two before the limit.
  const std::u16string title =
      std::u16string(kMaxQueryLength - 2, u'a') + u"\U0001F600";
  CreateHandlerWithTitle(title);

  auto results = Search(title + u"b");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0]->title_match_ranges,
              ElementsAre(gfx::Range(0, kMaxQueryLength)));
}

TEST_F(SettingsSearchHandlerTest, TruncationDropsSplitSurrogatePair) {
  // The limit falls between the emoji's two code units.
  const std::u16string title =
      std::u16string(kMaxQueryLength - 1, u'a') + u"\U0001F600";
  CreateHandlerWithTitle(title);

  auto results = Search(title);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0]->title_match_ranges,
              ElementsAre(gfx::Range(0, kMaxQueryLength - 1)));
}

TEST_F(SettingsSearchHandlerTest, EmptyItemListReturnsNothing) {
  CreateHandler({});
  EXPECT_THAT(Search(u"bookmarks"), IsEmpty());
}

}  // namespace

}  // namespace settings_search
