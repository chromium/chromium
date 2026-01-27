// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_accessibility.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_group.h"
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

}  // namespace tab_groups
