// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/tab_group_accessibility.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_group.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

namespace tab_groups {

std::u16string GetGroupContentString(const TabGroup* tab_group) {
  CHECK(tab_group);

  constexpr size_t kMaxTitleLength = 30;

  std::u16string format_string = l10n_util::GetPluralStringFUTF16(
      IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE, tab_group->tab_count() - 1);

  std::u16string short_title;
  gfx::ElideString(TabUIHelper::From(tab_group->GetFirstTab())->GetTitle(),
                   kMaxTitleLength, &short_title);

  return base::ReplaceStringPlaceholders(format_string, short_title, nullptr);
}

namespace {

std::u16string MakeICUFormattedTabTitleList(
    base::span<const std::u16string> items) {
  if (items.empty()) {
    return std::u16string();
  }
  std::vector<icu::UnicodeString> strings;
  strings.reserve(items.size());
  for (const auto& item : items) {
    strings.emplace_back(item.data(), item.size());
  }
  UErrorCode error = U_ZERO_ERROR;
  // Use UNITS type to avoid conjunctions like "and".
  // on the last item of the list.
  std::unique_ptr<icu::ListFormatter> formatter(
      icu::ListFormatter::createInstance(icu::Locale::getDefault(),
                                         ULISTFMT_TYPE_UNITS,
                                         ULISTFMT_WIDTH_WIDE, error));
  if (U_FAILURE(error) || !formatter) {
    return std::u16string();
  }
  icu::UnicodeString formatted;
  formatter->format(strings.data(), strings.size(), formatted, error);
  if (U_FAILURE(error)) {
    return std::u16string();
  }
  return base::i18n::UnicodeStringToString16(formatted);
}

}  // namespace

std::u16string GetHoverCardAccessibilityText(const tabs::TabGroupData& data) {
  const int num_tabs = data.num_tabs_in_group;
  const auto& tab_data = data.tab_data;

  // Prepare arguments for accessibility string, starting with the tab list.
  static constexpr int kAccessibleTabTitleMaxLength = 50;
  std::vector<std::u16string> formatted_titles;
  const int tabs_to_show =
      static_cast<int>(std::min(tab_data.size(), tabs::TabGroupData::kMaxTabs));
  for (int i = 0; i < tabs_to_show; ++i) {
    std::u16string bulleted_title =
        l10n_util::GetStringFUTF16(IDS_LIST_BULLET, tab_data[i].title);
    formatted_titles.emplace_back(gfx::TruncateString(
        bulleted_title, kAccessibleTabTitleMaxLength, gfx::CHARACTER_BREAK));
  }

  if (num_tabs > tabs_to_show) {
    int remaining = num_tabs - tabs_to_show;
    std::u16string remaining_string = l10n_util::GetStringFUTF16(
        IDS_TAB_GROUPS_HOVER_CARD_FOOTER, base::NumberToString16(remaining));
    formatted_titles.emplace_back(remaining_string);
  }

  std::u16string list_string = MakeICUFormattedTabTitleList(formatted_titles);

  std::u16string shared_state =
      data.is_sharing_group
          ? l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_SHARED)
          : u"";

  std::u16string collapsed_state =
      data.visual_data.is_collapsed()
          ? l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_COLLAPSED)
          : l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_EXPANDED);

  std::u16string group_title = data.visual_data.title();

  if (group_title.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
        {shared_state, list_string, collapsed_state}, nullptr);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT,
        {shared_state, group_title, list_string, collapsed_state}, nullptr);
  }
}

}  // namespace tab_groups
