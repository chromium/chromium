// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <string>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/windows_spell_checker.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"

class PlatformSpellChecker;

namespace spellcheck_platform {

bool SpellCheckerAvailable() {
  return true;
}

void PlatformSupportsLanguage(PlatformSpellChecker* spell_checker_instance,
                              const std::string& lang_tag,
                              base::OnceCallback<void(bool)> callback) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->IsLanguageSupported(lang_tag, std::move(callback));
}

void SetLanguage(PlatformSpellChecker* spell_checker_instance,
                 const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->CreateSpellChecker(lang_to_set, std::move(callback));
}

void DisableLanguage(PlatformSpellChecker* spell_checker_instance,
                     const std::string& lang_to_disable) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->DisableSpellChecker(lang_to_disable);
}

bool CheckSpelling(const base::string16& word_to_check, int tag) {
  return true;  // Not used in the Windows native spell checker.
}

void FillSuggestionList(const base::string16& wrong_word,
                        std::vector<base::string16>* optional_suggestions) {
  // Not used in the Windows native spell checker.
}

void RequestTextCheck(PlatformSpellChecker* spell_checker_instance,
                      int document_tag,
                      const base::string16& text,
                      TextCheckCompleteCallback callback) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->RequestTextCheck(document_tag, text, std::move(callback));
}

#if defined(OS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void GetPerLanguageSuggestions(PlatformSpellChecker* spell_checker_instance,
                               const base::string16& word,
                               GetSuggestionsCallback callback) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->GetPerLanguageSuggestions(word, std::move(callback));
}
#endif  // defined(OS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

void AddWord(PlatformSpellChecker* spell_checker_instance,
             const base::string16& word) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->AddWordForAllLanguages(word);
}

void RemoveWord(PlatformSpellChecker* spell_checker_instance,
                const base::string16& word) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->RemoveWordForAllLanguages(word);
}

void IgnoreWord(PlatformSpellChecker* spell_checker_instance,
                const base::string16& word) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->IgnoreWordForAllLanguages(word);
}

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
  // Not used in Windows
}

void RetrieveSpellcheckLanguages(
    PlatformSpellChecker* spell_checker_instance,
    RetrieveSpellcheckLanguagesCompleteCallback callback) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->RetrieveSpellcheckLanguages(std::move(callback));
}

void AddSpellcheckLanguagesForTesting(
    PlatformSpellChecker* spell_checker_instance,
    const std::vector<std::string>& languages) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->AddSpellcheckLanguagesForTesting(languages);
}

int GetDocumentTag() {
  return 1;  // Not used in Windows
}

void CloseDocumentWithTag(int tag) {
  // Not implemented since Windows spellchecker doesn't have this concept
}

bool SpellCheckerProvidesPanel() {
  return false;  // Windows doesn't have a spelling panel
}

bool SpellingPanelVisible() {
  return false;  // Windows doesn't have a spelling panel
}

void ShowSpellingPanel(bool show) {
  // Not implemented since Windows doesn't have spelling panel like Mac
}

void UpdateSpellingPanelWithMisspelledWord(const base::string16& word) {
  // Not implemented since Windows doesn't have spelling panel like Mac
}

void RecordChromeLocalesStats(PlatformSpellChecker* spell_checker_instance,
                              const std::vector<std::string> chrome_locales,
                              SpellCheckHostMetrics* metrics) {
  if (spellcheck::WindowsVersionSupportsSpellchecker()) {
    static_cast<WindowsSpellChecker*>(spell_checker_instance)
        ->RecordChromeLocalesStats(std::move(chrome_locales), metrics);
  }
}

void RecordSpellcheckLocalesStats(
    PlatformSpellChecker* spell_checker_instance,
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  if (spellcheck::WindowsVersionSupportsSpellchecker()) {
    static_cast<WindowsSpellChecker*>(spell_checker_instance)
        ->RecordSpellcheckLocalesStats(std::move(spellcheck_locales), metrics);
  }
}

}  // namespace spellcheck_platform
