// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_provider.h"

#include "base/i18n/string_search.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_metadata.h"
#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_result.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {

SettingsShortcutProvider::SettingsShortcutProvider(Profile* profile)
    : profile_(profile) {}

void SettingsShortcutProvider::Start(const std::u16string& query) {
  SearchProvider::Results search_results;
  // TODO(wutao): Use tokenized string match.
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents finder(query);
  for (const auto& shortcut : GetSettingsShortcutList()) {
    int searchable_string_id = shortcut.searchable_string_resource_id;
    if (finder.Search(
            l10n_util::GetStringUTF16(shortcut.name_string_resource_id),
            nullptr, nullptr) ||
        (searchable_string_id != 0 &&
         finder.Search(l10n_util::GetStringUTF16(searchable_string_id), nullptr,
                       nullptr))) {
      search_results.emplace_back(
          std::make_unique<SettingsShortcutResult>(profile_, shortcut));
      search_results.back()->set_relevance(1.0);
    }
  }
  SwapResults(&search_results);
}

ash::AppListSearchResultType SettingsShortcutProvider::ResultType() {
  return ash::AppListSearchResultType::kUnknown;
}

}  // namespace app_list
