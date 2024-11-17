// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface that any platform-specific spellchecker
// needs to implement in order to be used by the browser.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_PLATFORM_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_PLATFORM_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "components/spellcheck/common/spellcheck_common.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

class PlatformSpellChecker;

struct SpellCheckResult;

namespace spellcheck_platform {

typedef base::OnceCallback<void(const std::vector<SpellCheckResult>&)>
    TextCheckCompleteCallback;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
typedef base::OnceCallback<void(const spellcheck::PerLanguageSuggestions&)>
    GetSuggestionsCallback;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

typedef base::OnceCallback<void(const std::vector<std::string>& /* results */)>
    RetrieveSpellcheckLanguagesCompleteCallback;

// Get the languages supported by the platform spellchecker and store them in
// |spellcheck_languages|. Note that they must be converted to
// Chromium style codes (en-US not en_US). See spellchecker.cc for a full list.
void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages);

// Retrieve BCP47 language tags for registered platform spellcheckers
// on the system. Callback will pass an empty vector of language tags if the OS
// does not support spellcheck or this functionality is not yet implemented.
void RetrieveSpellcheckLanguages(
    PlatformSpellChecker* spell_checker_instance,
    RetrieveSpellcheckLanguagesCompleteCallback callback);

// Test-only method for adding fake list of platform spellcheck languages.
void AddSpellcheckLanguagesForTesting(
    PlatformSpellChecker* spell_checker_instance,
    const std::vector<std::string>& languages);

// Returns the language used for spellchecking on the platform.
std::string GetSpellCheckerLanguage();

// Returns true if there is a platform-specific spellchecker that can be used.
bool SpellCheckerAvailable();

// Returns true if the platform spellchecker has a spelling panel.
bool SpellCheckerProvidesPanel();

// Returns true if the platform spellchecker panel is visible.
bool SpellingPanelVisible();

// Shows the spelling panel if |show| is true and hides it if it is not.
void ShowSpellingPanel(bool show);

// Changes the word show in the spelling panel to be |word|. Note that the
// spelling panel need not be displayed for this to work.
void UpdateSpellingPanelWithMisspelledWord(const std::u16string& word);

// Asynchronously checks whether the current system's spellchecker supports the
// given language. If the platform-specific spellchecker supports the language,
// then the callback is invoked with true, otherwise it is invoked with false.
void PlatformSupportsLanguage(PlatformSpellChecker* spell_checker_instance,
                              const std::string& current_language,
                              base::OnceCallback<void(bool)> callback);

// Sets the language for the platform-specific spellchecker asynchronously. The
// callback will be invoked with boolean parameter indicating the status of the
// spellchecker for the language |lang_to_set|.
void SetLanguage(PlatformSpellChecker* spell_checker_instance,
                 const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback);

// Removes the language for the platform-specific spellchecker.
void DisableLanguage(PlatformSpellChecker* spell_checker_instance,
                     const std::string& lang_to_disable);

// Checks the spelling of the given string, using the platform-specific
// spellchecker. Returns true if the word is spelled correctly.
bool CheckSpelling(const std::u16string& word_to_check, int tag);

// Fills the given vector |optional_suggestions| with a number (up to
// kMaxSuggestions, which is defined in spellchecker_common.h) of suggestions
// for the string |wrong_word|.
void FillSuggestionList(const std::u16string& wrong_word,
                        std::vector<std::u16string>* optional_suggestions);

// Adds the given word to the platform dictionary.
void AddWord(PlatformSpellChecker* spell_checker_instance,
             const std::u16string& word);

// Remove a given word from the platform dictionary.
void RemoveWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word);

// Gets a unique tag to identify a document. Used in ignoring words.
int GetDocumentTag();

// Tells the platform spellchecker to ignore a word. This doesn't take a tag
// because in most of the situations in which it is called, the only way to know
// the tag for sure is to ask the renderer, which would mean blocking in the
// browser, so (on the mac, anyway) we remember the most recent tag and use
// it, since it should always be from the same document.
void IgnoreWord(PlatformSpellChecker* spell_checker_instance,
                const std::u16string& word);

// Tells the platform spellchecker that a document associated with a tag has
// closed. Generally, this means that any ignored words associated with that
// document can now be forgotten.
void CloseDocumentWithTag(int tag);

// Requests an asynchronous spell and grammar checking.
void RequestTextCheck(PlatformSpellChecker* spell_checker_instance,
                      int document_tag,
                      const std::u16string& text,
                      TextCheckCompleteCallback callback);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// Finds the replacement suggestions for each language for the given word.
void GetPerLanguageSuggestions(PlatformSpellChecker* spell_checker_instance,
                               const std::u16string& word,
                               GetSuggestionsCallback callback);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(IS_WIN)
// Records statistics about spell check support for the user's Chrome locales.
void RecordChromeLocalesStats(PlatformSpellChecker* spell_checker_instance,
                              std::vector<std::string> chrome_locales);

// Records statistics about which spell checker supports which of the user's
// enabled spell check locales.
void RecordSpellcheckLocalesStats(PlatformSpellChecker* spell_checker_instance,
                                  std::vector<std::string> spellcheck_locales);
#endif  // BUILDFLAG(IS_WIN)

// Internal state, to restore system state after testing.
// Not public since it contains Cocoa data types.
class SpellcheckerStateInternal;

// Test helper, forces the system spellchecker to en-US for its lifetime.
class ScopedEnglishLanguageForTest {
 public:
  ScopedEnglishLanguageForTest();
  ~ScopedEnglishLanguageForTest();
 private:
  raw_ptr<SpellcheckerStateInternal, DanglingUntriaged> state_;
};

}  // namespace spellcheck_platform

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_PLATFORM_H_
