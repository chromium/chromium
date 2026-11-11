// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_section.h"

#include <memory>

#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings_search {

namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

TEST(SettingsSectionTest, SplitSynonymsAsciiCommasAndWhitespace) {
  EXPECT_THAT(SplitSynonyms(u"house, icon,  toolbar , homepage "),
              ElementsAre(u"house", u"icon", u"toolbar", u"homepage"));
}

TEST(SettingsSectionTest, SplitSynonymsLocalizedCommasAndWhitespace) {
  // Covers Arabic ('،' U+060C), ideographic ('、' U+3001), and fullwidth
  // ('，' U+FF0C) commas, as well as ideographic whitespace (U+3000).
  EXPECT_THAT(SplitSynonyms(u"مظهر، أيقونة、\u3000テーマ\u3000，深色模式"),
              ElementsAre(u"مظهر", u"أيقونة", u"テーマ", u"深色模式"));
}

TEST(SettingsSectionTest, SplitSynonymsSkipsEmptySegments) {
  EXPECT_THAT(SplitSynonyms(u""), IsEmpty());
  EXPECT_THAT(SplitSynonyms(u" , ، 、 ， \u3000 "), IsEmpty());
  EXPECT_THAT(SplitSynonyms(u",a,,b،c、、d，，"),
              ElementsAre(u"a", u"b", u"c", u"d"));
}

TEST(SettingsSectionTest, GetSynonymsFromResource) {
  EXPECT_THAT(GetSynonyms(IDS_SETTINGS_TAG_SHOW_HOME_BUTTON),
              ElementsAre(u"house", u"icon", u"toolbar", u"homepage", u"start",
                          u"url", u"shortcut"));
}

TEST(SettingsSectionTest, MakeSearchItemResolvesStrings) {
  constexpr int kBreadcrumbIds[] = {IDS_SETTINGS_APPEARANCE,
                                    IDS_SETTINGS_CUSTOMIZE_FONTS};
  std::unique_ptr<SettingsSearchItem> item = MakeSearchItem(
      mojom::SettingId::kShowHomeButton, "fonts", IDS_SETTINGS_SHOW_HOME_BUTTON,
      kBreadcrumbIds, IDS_SETTINGS_TAG_SHOW_HOME_BUTTON);

  EXPECT_EQ(mojom::SettingId::kShowHomeButton, item->id());
  EXPECT_EQ("/fonts", item->route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SHOW_HOME_BUTTON),
            item->GetTitle());
  EXPECT_THAT(
      item->breadcrumbs(),
      ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_APPEARANCE),
                  l10n_util::GetStringUTF16(IDS_SETTINGS_CUSTOMIZE_FONTS)));
  EXPECT_THAT(item->GetSynonyms(),
              ElementsAre(u"house", u"icon", u"toolbar", u"homepage", u"start",
                          u"url", u"shortcut"));
}

TEST(SettingsSectionTest, MakeSearchItemWithoutBreadcrumbsOrSynonyms) {
  std::unique_ptr<SettingsSearchItem> item =
      MakeSearchItem(mojom::SettingId::kAppearance, "appearance",
                     IDS_SETTINGS_APPEARANCE, /*breadcrumb_ids=*/{});

  EXPECT_EQ(mojom::SettingId::kAppearance, item->id());
  EXPECT_EQ("/appearance", item->route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_APPEARANCE),
            item->GetTitle());
  EXPECT_THAT(item->breadcrumbs(), IsEmpty());
  EXPECT_THAT(item->GetSynonyms(), IsEmpty());
}

// A page's own item has no breadcrumbs but can have synonyms, as the
// Appearance page's item does.
TEST(SettingsSectionTest, MakeSearchItemWithSynonymsButNoBreadcrumbs) {
  std::unique_ptr<SettingsSearchItem> item = MakeSearchItem(
      mojom::SettingId::kAppearance, "appearance", IDS_SETTINGS_APPEARANCE,
      /*breadcrumb_ids=*/{}, IDS_SETTINGS_TAG_APPEARANCE);

  EXPECT_THAT(item->breadcrumbs(), IsEmpty());
  EXPECT_THAT(item->GetSynonyms(),
              ElementsAreArray(GetSynonyms(IDS_SETTINGS_TAG_APPEARANCE)));
}

}  // namespace

}  // namespace settings_search
