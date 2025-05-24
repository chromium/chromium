// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <string>

#include "base/functional/callback.h"
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
  if (!spell_checker_instance) {
    std::move(callback).Run(false);
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->IsLanguageSupported(lang_tag, std::move(callback));
}

void SetLanguage(PlatformSpellChecker* spell_checker_instance,
                 const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback) {
  if (!spell_checker_instance) {
    std::move(callback).Run(false);
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->CreateSpellChecker(lang_to_set, std::move(callback));
}

void DisableLanguage(PlatformSpellChecker* spell_checker_instance,
                     const std::string& lang_to_disable) {
  if (!spell_checker_instance) {
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->DisableSpellChecker(lang_to_disable);
}

bool CheckSpelling(const std::u16string& word_to_check, int tag) {
  return true;  // Not used in the Windows native spell checker.
}

void FillSuggestionList(const std::u16string& wrong_word,
                        std::vector<std::u16string>* optional_suggestions) {
  // Not used in the Windows native spell checker.
}

void RequestTextCheck(PlatformSpellChecker* spell_checker_instance,
                      int document_tag,
                      const std::u16string& text,
                      TextCheckCompleteCallback callback) {
  if (!spell_checker_instance) {
    std::move(callback).Run(std::vector<SpellCheckResult>());
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->RequestTextCheck(document_tag, text, std::move(callback));
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void GetPerLanguageSuggestions(PlatformSpellChecker* spell_checker_instance,
                               const std::u16string& word,
                               GetSuggestionsCallback callback) {
  if (!spell_checker_instance) {
    std::move(callback).Run(spellcheck::PerLanguageSuggestions());
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->GetPerLanguageSuggestions(word, std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

void AddWord(PlatformSpellChecker* spell_checker_instance,
             const std::u16string& word) {
  if (!spell_checker_instance) {
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->AddWordForAllLanguages(word);
}

void RemoveWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word) {
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->RemoveWordForAllLanguages(word);
}

void IgnoreWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word) {
  if (!spell_checker_instance) {
    return;
  }
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
  if (!spell_checker_instance) {
    return;
  }
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

void UpdateSpellingPanelWithMisspelledWord(const std::u16string& word) {
  // Not implemented since Windows doesn't have spelling panel like Mac
}

void RecordChromeLocalesStats(PlatformSpellChecker* spell_checker_instance,
                              std::vector<std::string> chrome_locales) {
  if (!spell_checker_instance) {
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->RecordChromeLocalesStats(std::move(chrome_locales));
}

void RecordSpellcheckLocalesStats(PlatformSpellChecker* spell_checker_instance,
                                  std::vector<std::string> spellcheck_locales) {
  if (!spell_checker_instance) {
    return;
  }
  static_cast<WindowsSpellChecker*>(spell_checker_instance)
      ->RecordSpellcheckLocalesStats(std::move(spellcheck_locales));
}

}  // namespace spellcheck_platform
