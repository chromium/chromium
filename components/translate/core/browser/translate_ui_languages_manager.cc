// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ui_languages_manager.h"

#include <algorithm>

#include "base/i18n/string_compare.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace translate {

TranslateUILanguagesManager::TranslateUILanguagesManager(
    const base::WeakPtr<TranslateManager>& translate_manager,
    const std::vector<std::string>& language_codes,
    const std::string& source_language,
    const std::string& target_language)
    : translate_manager_(translate_manager),
      source_language_index_(kNoIndex),
      initial_source_language_index_(kNoIndex),
      target_language_index_(kNoIndex),
      prefs_(translate_manager_->translate_client()->GetTranslatePrefs()) {
  std::string locale =
      TranslateDownloadManager::GetInstance()->application_locale();

  // Reserve additional space for unknown language option.
  std::vector<std::string>::size_type languages_size = language_codes.size();
  languages_size += 1;
  languages_.reserve(languages_size);

  // Preparing for the alphabetical order in the locale.
  std::unique_ptr<icu::Collator> collator = CreateCollator(locale);
  for (const std::string& language_code : language_codes) {
    std::u16string language_name =
        l10n_util::GetDisplayNameForLocale(language_code, locale, true);
    languages_.emplace_back(std::move(language_code), std::move(language_name));
  }

  // Sort |languages_| in alphabetical order according to the display name.
  std::sort(
      languages_.begin(), languages_.end(),
      [&collator](const LanguageNamePair& lhs, const LanguageNamePair& rhs) {
        if (collator) {
          switch (base::i18n::CompareString16WithCollator(*collator, lhs.second,
                                                          rhs.second)) {
            case UCOL_LESS:
              return true;
            case UCOL_GREATER:
              return false;
            case UCOL_EQUAL:
              break;
          }
        } else {
          // |locale| may not be supported by ICU collator (crbug/54833). In
          // this case, let's order the languages in UTF-8.
          int result = lhs.second.compare(rhs.second);
          if (result != 0) {
            return result < 0;
          }
        }
        // Matching display names will be ordered alphabetically according to
        // the language codes.
        return lhs.first < rhs.first;
      });

  languages_.emplace_back(kUnknownLanguageCode,
                          GetUnknownLanguageDisplayName());
  std::rotate(languages_.rbegin(), languages_.rbegin() + 1, languages_.rend());

  for (std::vector<LanguageNamePair>::const_iterator iter = languages_.begin();
       iter != languages_.end(); ++iter) {
    const std::string& language_code = iter->first;
    if (language_code == source_language) {
      source_language_index_ = iter - languages_.begin();
    }
    if (language_code == target_language) {
      target_language_index_ = iter - languages_.begin();
    }
  }
}

TranslateUILanguagesManager::~TranslateUILanguagesManager() = default;

std::unique_ptr<icu::Collator> TranslateUILanguagesManager::CreateCollator(
    const std::string& locale) {
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale loc(locale.c_str());
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(loc, error));
  if (!collator || !U_SUCCESS(error)) {
    return nullptr;
  }
  collator->setStrength(icu::Collator::PRIMARY);
  return collator;
}

size_t TranslateUILanguagesManager::GetNumberOfLanguages() const {
  return languages_.size();
}

std::string TranslateUILanguagesManager::GetSourceLanguageCode() const {
  return (GetSourceLanguageIndex() == kNoIndex)
             ? translate::kUnknownLanguageCode
             : GetLanguageCodeAt(GetSourceLanguageIndex());
}

std::string TranslateUILanguagesManager::GetTargetLanguageCode() const {
  return (GetTargetLanguageIndex() == kNoIndex)
             ? translate::kUnknownLanguageCode
             : GetLanguageCodeAt(GetTargetLanguageIndex());
}

std::string TranslateUILanguagesManager::GetLanguageCodeAt(size_t index) const {
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].first;
}

std::u16string TranslateUILanguagesManager::GetLanguageNameAt(
    size_t index) const {
  if (index == kNoIndex) {
    return std::u16string();
  }
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].second;
}

bool TranslateUILanguagesManager::UpdateSourceLanguageIndex(
    size_t language_index) {
  DCHECK_LT(language_index, GetNumberOfLanguages());
  if (source_language_index_ == language_index) {
    return false;
  }

  source_language_index_ = language_index;
  return true;
}

bool TranslateUILanguagesManager::UpdateSourceLanguage(
    const std::string& language_code) {
  if (GetSourceLanguageCode() == language_code) {
    return false;
  }
  for (size_t i = 0; i < languages_.size(); ++i) {
    if (languages_[i].first.compare(language_code) == 0) {
      UpdateSourceLanguageIndex(i);
      return true;
    }
  }

  return false;
}

bool TranslateUILanguagesManager::UpdateTargetLanguageIndex(
    size_t language_index) {
  DCHECK_LT(language_index, GetNumberOfLanguages());
  if (target_language_index_ == language_index) {
    return false;
  }

  target_language_index_ = language_index;
  return true;
}

bool TranslateUILanguagesManager::UpdateTargetLanguage(
    const std::string& language_code) {
  if (GetTargetLanguageCode() == language_code) {
    return false;
  }
  for (size_t i = 0; i < languages_.size(); ++i) {
    if (languages_[i].first.compare(language_code) == 0) {
      UpdateTargetLanguageIndex(i);
      return true;
    }
  }

  return false;
}

// static
std::u16string TranslateUILanguagesManager::GetUnknownLanguageDisplayName() {
  return l10n_util::GetStringUTF16(IDS_TRANSLATE_DETECTED_LANGUAGE);
}

}  // namespace translate
