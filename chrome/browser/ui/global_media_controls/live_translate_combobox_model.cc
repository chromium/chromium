// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/live_translate_combobox_model.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

LiveTranslateComboboxModel::LiveTranslateComboboxModel(Profile* profile)
    : profile_(profile) {
  std::vector<translate::TranslateLanguageInfo> language_list;
  translate::TranslatePrefs::GetLanguageInfoList(
      g_browser_process->GetApplicationLocale(), true, &language_list);

  for (const auto& language : language_list) {
    if (language.supports_translate &&
        language.code == l10n_util::GetLanguage(language.code)) {
      languages_.push_back(language);
    }
  }
}

LiveTranslateComboboxModel::~LiveTranslateComboboxModel() = default;

size_t LiveTranslateComboboxModel::GetItemCount() const {
  return languages_.size();
}

std::u16string LiveTranslateComboboxModel::GetItemAt(size_t index) const {
  return base::UTF8ToUTF16(languages_[index].display_name);
}

absl::optional<size_t> LiveTranslateComboboxModel::GetDefaultIndex() const {
  std::string target_language =
      profile_->GetPrefs()->GetString(prefs::kLiveTranslateTargetLanguageCode);
  for (size_t i = 0; i < languages_.size(); i++) {
    if (target_language == languages_[i].code) {
      return i;
    }
  }

  return 0;
}

void LiveTranslateComboboxModel::UpdateTargetLanguageIndex(int index) {
  profile_->GetPrefs()->SetString(prefs::kLiveTranslateTargetLanguageCode,
                                  languages_[index].code);
}
