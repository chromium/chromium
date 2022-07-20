// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/spell_checker.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {
namespace {

}  // namespace

SpellChecker::SpellChecker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  quick_answers_state_observation_.Observe(QuickAnswersState::Get());
}

SpellChecker::~SpellChecker() {
  spellcheck_languages_.clear();
}

void SpellChecker::CheckSpelling(const std::string& word,
                                 CheckSpellingCallback callback) {
  auto iterator = spellcheck_languages_.begin();
  if (iterator == spellcheck_languages_.end()) {
    std::move(callback).Run(false, std::string());
    return;
  }

  iterator->get()->CheckSpelling(
      word, base::BindOnce(&SpellChecker::CollectResults,
                           base::Unretained(this), word, std::move(callback),
                           iterator, languages_list_version_));
}

void SpellChecker::OnSettingsEnabled(bool enabled) {
  feature_enabled_ = enabled;

  OnStateUpdated();
}

void SpellChecker::OnApplicationLocaleReady(const std::string& locale) {
  application_locale_ = locale;

  OnStateUpdated();
}

void SpellChecker::OnPreferredLanguagesChanged(
    const std::string& preferred_languages) {
  preferred_languages_ = preferred_languages;

  OnStateUpdated();
}

void SpellChecker::OnEligibilityChanged(bool eligible) {
  feature_eligible_ = eligible;

  OnStateUpdated();
}

void SpellChecker::OnStateUpdated() {
  if (!feature_eligible_.has_value() || !feature_enabled_.has_value() ||
      !application_locale_.has_value() || !preferred_languages_.has_value()) {
    // Still waiting for all of the states to be ready.
    return;
  }

  if (!feature_eligible_.value() || !feature_enabled_.value()) {
    spellcheck_languages_.clear();
    languages_list_version_++;
    return;
  }

  // Add application language.
  std::set<std::string> languages;
  languages.insert(l10n_util::GetLanguage(application_locale_.value()));

  // Add preferred languages if supported.
  if (chromeos::features::IsQuickAnswersForMoreLocalesEnabled()) {
    auto preferred_languages_list =
        base::SplitString(preferred_languages_.value(), ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const std::string& locale : preferred_languages_list) {
      auto language = l10n_util::GetLanguage(locale);
      if (QuickAnswersState::Get()->IsSupportedLanguage(language))
        languages.insert(language);
    }
  }

  spellcheck_languages_.clear();
  languages_list_version_++;
  for (auto language : languages) {
    spellcheck_languages_.push_back(
        std::make_unique<SpellCheckLanguage>(url_loader_factory_));
    spellcheck_languages_.back()->Initialize(language);
  }
}

void SpellChecker::CollectResults(const std::string& word,
                                  CheckSpellingCallback callback,
                                  SpellCheckLanguageIterator iterator,
                                  int languages_list_version,
                                  bool is_correct) {
  if (is_correct) {
    std::move(callback).Run(true, iterator->get()->language());
    return;
  }

  // The languages list has been updated, return false for the current call.
  if (languages_list_version != languages_list_version_) {
    std::move(callback).Run(false, std::string());
    return;
  }

  iterator++;
  if (iterator == spellcheck_languages_.end()) {
    std::move(callback).Run(false, std::string());
    return;
  }

  iterator->get()->CheckSpelling(
      word, base::BindOnce(&SpellChecker::CollectResults,
                           base::Unretained(this), word, std::move(callback),
                           iterator, languages_list_version));
}

}  // namespace quick_answers
