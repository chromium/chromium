// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/spellcheck/common/spellcheck_features.h"

class PlatformSpellChecker;

namespace spellcheck_platform {

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
}

void RetrieveSpellcheckLanguages(
    PlatformSpellChecker* spell_checker_instance,
    RetrieveSpellcheckLanguagesCompleteCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(std::vector<std::string>());
}

void AddSpellcheckLanguagesForTesting(
    PlatformSpellChecker* spell_checker_instance,
    const std::vector<std::string>& languages) {
  NOTIMPLEMENTED();
}

std::string GetSpellCheckerLanguage() {
  return std::string();
}

bool SpellCheckerAvailable() {
  return spellcheck::IsAndroidSpellCheckFeatureEnabled();
}

bool SpellCheckerProvidesPanel() {
  return false;
}

bool SpellingPanelVisible() {
  return false;
}

void ShowSpellingPanel(bool show) {
}

void UpdateSpellingPanelWithMisspelledWord(const std::u16string& word) {}

void PlatformSupportsLanguage(PlatformSpellChecker* spell_checker_instance,
                              const std::string& current_language,
                              base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void SetLanguage(PlatformSpellChecker* spell_checker_instance,
                 const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DisableLanguage(PlatformSpellChecker* spell_checker_instance,
                     const std::string& lang_to_disable) {}

bool CheckSpelling(const std::u16string& word_to_check, int tag) {
  return true;
}

void FillSuggestionList(const std::u16string& wrong_word,
                        std::vector<std::u16string>* optional_suggestions) {}

void AddWord(PlatformSpellChecker* spell_checker_instance,
             const std::u16string& word) {}

void RemoveWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word) {}

int GetDocumentTag() {
  return 1;
}

void IgnoreWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word) {}

void CloseDocumentWithTag(int tag) {
}

void RequestTextCheck(PlatformSpellChecker* spell_checker_instance,
                      int document_tag,
                      const std::u16string& text,
                      TextCheckCompleteCallback callback) {}

}  // namespace spellcheck_platform
