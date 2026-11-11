// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_section.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings_search {

std::vector<std::u16string> SplitSynonyms(std::u16string_view synonyms) {
  return base::SplitString(synonyms, u",،、，", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::vector<std::u16string> GetSynonyms(int message_id) {
  return SplitSynonyms(l10n_util::GetStringUTF16(message_id));
}

std::unique_ptr<SettingsSearchItem> MakeSearchItem(
    mojom::SettingId id,
    std::string_view sub_page,
    int title_id,
    base::span<const int> breadcrumb_ids,
    std::optional<int> synonyms_id) {
  std::vector<std::u16string> breadcrumbs;
  breadcrumbs.reserve(breadcrumb_ids.size());
  for (int breadcrumb_id : breadcrumb_ids) {
    breadcrumbs.push_back(l10n_util::GetStringUTF16(breadcrumb_id));
  }
  return std::make_unique<SettingsSearchItem>(
      id, base::StrCat({"/", sub_page}), l10n_util::GetStringUTF16(title_id),
      std::move(breadcrumbs),
      synonyms_id ? GetSynonyms(*synonyms_id) : std::vector<std::u16string>());
}

SettingsSection::SettingsSection(SettingsSearchRegistry* registry)
    : registry_(registry) {
  CHECK(registry_);
}

SettingsSection::~SettingsSection() = default;

}  // namespace settings_search
