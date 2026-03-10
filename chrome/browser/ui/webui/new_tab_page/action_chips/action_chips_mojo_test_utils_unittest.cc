// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_mojo_test_utils.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace action_chips::mojom {
namespace {

TEST(ActionChipsMojoTestUtilsTest, PrintTabInfo) {
  auto tab = TabInfo::New();
  tab->tab_id = 123;
  tab->title = "Test Tab";
  tab->url = GURL("https://example.com");
  tab->last_active_time = base::Time::FromMillisecondsSinceUnixEpoch(456);

  EXPECT_EQ(::testing::PrintToString(tab),
            "TabInfo{\n"
            "  tab_id: 123,\n"
            "  title: \"Test Tab\",\n"
            "  url: \"https://example.com/\",\n"
            "  last_active_time: 1970-01-01 00:00:00.456000 UTC\n"
            "}");
}

TEST(ActionChipsMojoTestUtilsTest, PrintFormattedString) {
  auto str = FormattedString::New();
  str->text = "Primary Text";
  str->a11y_text = "Accessible Text";

  EXPECT_EQ(::testing::PrintToString(str),
            "FormattedString{\n"
            "  text: \"Primary Text\",\n"
            "  a11y_text: \"Accessible Text\"\n"
            "}");

  auto str_no_a11y = FormattedString::New();
  str_no_a11y->text = "Primary Text";

  EXPECT_EQ(::testing::PrintToString(str_no_a11y),
            "FormattedString{\n"
            "  text: \"Primary Text\",\n"
            "  a11y_text: null\n"
            "}");
}

TEST(ActionChipsMojoTestUtilsTest, PrintSuggestTemplateInfo) {
  auto info = SuggestTemplateInfo::New();
  info->type_icon = static_cast<action_chips::mojom::IconType>(1);
  info->primary_text = FormattedString::New("Primary", "Primary A11y");
  info->secondary_text = FormattedString::New("Secondary", std::nullopt);

  EXPECT_EQ(::testing::PrintToString(info),
            "SuggestTemplateInfo{\n"
            "  type_icon: kHistory,\n"
            "  primary_text: FormattedString{\n"
            "    text: \"Primary\",\n"
            "    a11y_text: \"Primary A11y\"\n"
            "  },\n"
            "  secondary_text: FormattedString{\n"
            "    text: \"Secondary\",\n"
            "    a11y_text: null\n"
            "  }\n"
            "}");

  auto info_missing = SuggestTemplateInfo::New();
  info_missing->type_icon = static_cast<action_chips::mojom::IconType>(2);

  EXPECT_EQ(::testing::PrintToString(info_missing),
            "SuggestTemplateInfo{\n"
            "  type_icon: kSearchLoop,\n"
            "  primary_text: nullptr,\n"
            "  secondary_text: nullptr\n"
            "}");
}

TEST(ActionChipsMojoTestUtilsTest, PrintActionChip) {
  auto chip = ActionChip::New();
  chip->suggestion = "Example Suggestion";
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon =
      static_cast<action_chips::mojom::IconType>(3);
  chip->suggest_template_info->primary_text =
      FormattedString::New("Primary", std::nullopt);
  chip->tab = TabInfo::New();
  chip->tab->tab_id = 456;
  chip->tab->title = "Tab Title";
  chip->tab->url = GURL("https://tab.com/");
  chip->tab->last_active_time = base::Time::FromMillisecondsSinceUnixEpoch(789);

  EXPECT_EQ(::testing::PrintToString(chip),
            "ActionChip{\n"
            "  suggestion: \"Example Suggestion\",\n"
            "  suggest_template_info: SuggestTemplateInfo{\n"
            "    type_icon: kSearchLoopWithSparkle,\n"
            "    primary_text: FormattedString{\n"
            "      text: \"Primary\",\n"
            "      a11y_text: null\n"
            "    },\n"
            "    secondary_text: nullptr\n"
            "  },\n"
            "  tab_info: TabInfo{\n"
            "    tab_id: 456,\n"
            "    title: \"Tab Title\",\n"
            "    url: \"https://tab.com/\",\n"
            "    last_active_time: 1970-01-01 00:00:00.789000 UTC\n"
            "  }\n"
            "}");
}

}  // namespace
}  // namespace action_chips::mojom
