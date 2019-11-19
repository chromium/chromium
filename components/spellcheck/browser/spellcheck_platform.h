// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface that any platform-specific spellchecker
// needs to implement in order to be used by the browser.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_PLATFORM_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_PLATFORM_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#endif  // defined(OS_WIN)

struct SpellCheckResult;

namespace spellcheck_platform {

typedef base::OnceCallback<void(
    const std::vector<SpellCheckResult>& /* results */)>
    TextCheckCompleteCallback;

// Get the languages supported by the platform spellchecker and store them in
// |spellcheck_languages|. Note that they must be converted to
// Chromium style codes (en-US not en_US). See spellchecker.cc for a full list.
void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages);

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
void UpdateSpellingPanelWithMisspelledWord(const base::string16& word);

// Translates the codes used by chrome to the language codes used by os x
// and checks the given language agains the languages that the current system
// supports. If the platform-specific spellchecker supports the language,
// then returns true, otherwise false.
bool PlatformSupportsLanguage(const std::string& current_language);

// Sets the language for the platform-specific spellchecker asynchronously. The
// callback will be invoked with boolean parameter indicating the status of the
// spellchecker for the language |lang_to_set|.
void SetLanguage(const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback);

// Removes the language for the platform-specific spellchecker.
void DisableLanguage(const std::string& lang_to_disable);

// Checks the spelling of the given string, using the platform-specific
// spellchecker. Returns true if the word is spelled correctly.
bool CheckSpelling(const base::string16& word_to_check, int tag);

// Fills the given vector |optional_suggestions| with a number (up to
// kMaxSuggestions, which is defined in spellchecker_common.h) of suggestions
// for the string |wrong_word|.
void FillSuggestionList(const base::string16& wrong_word,
                        std::vector<base::string16>* optional_suggestions);

// Adds the given word to the platform dictionary.
void AddWord(const base::string16& word);

// Remove a given word from the platform dictionary.
void RemoveWord(const base::string16& word);

// Gets a unique tag to identify a document. Used in ignoring words.
int GetDocumentTag();

// Tells the platform spellchecker to ignore a word. This doesn't take a tag
// because in most of the situations in which it is called, the only way to know
// the tag for sure is to ask the renderer, which would mean blocking in the
// browser, so (on the mac, anyway) we remember the most recent tag and use
// it, since it should always be from the same document.
void IgnoreWord(const base::string16& word);

// Tells the platform spellchecker that a document associated with a tag has
// closed. Generally, this means that any ignored words associated with that
// document can now be forgotten.
void CloseDocumentWithTag(int tag);

// Requests an asyncronous spell and grammar checking.
void RequestTextCheck(int document_tag,
                      const base::string16& text,
                      TextCheckCompleteCallback callback);

#if defined(OS_WIN)
// Records how many user spellcheck languages are currently not supported by the
// Windows OS spellchecker due to missing language packs.
void RecordMissingLanguagePacksCount(
    std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics);
#endif  // defined(OS_WIN)

// Internal state, to restore system state after testing.
// Not public since it contains Cocoa data types.
class SpellcheckerStateInternal;

// Test helper, forces the system spellchecker to en-US for its lifetime.
class ScopedEnglishLanguageForTest {
 public:
  ScopedEnglishLanguageForTest();
  ~ScopedEnglishLanguageForTest();
 private:
  SpellcheckerStateInternal* state_;
};

}  // namespace spellcheck_platform

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_PLATFORM_H_
