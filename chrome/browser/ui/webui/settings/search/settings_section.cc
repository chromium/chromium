// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_section.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/strings/string_split.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings_search {

std::vector<std::u16string> SplitSynonyms(std::u16string_view synonyms) {
  return base::SplitString(synonyms, u",،、，", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::vector<std::u16string> GetSynonyms(int message_id) {
  return SplitSynonyms(l10n_util::GetStringUTF16(message_id));
}

SettingsSection::SettingsSection(SettingsSearchRegistry* registry)
    : registry_(registry) {
  CHECK(registry_);
}

SettingsSection::~SettingsSection() = default;

}  // namespace settings_search
