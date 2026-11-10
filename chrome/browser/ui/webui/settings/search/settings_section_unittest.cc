// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_section.h"

#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_search {

namespace {

using ::testing::ElementsAre;
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

}  // namespace

}  // namespace settings_search
