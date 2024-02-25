// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_LANGUAGES_MANAGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_LANGUAGES_MANAGER_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace translate {

class TranslateManager;
class TranslatePrefs;

// Handles index management and querying functions for language lists used in
// the Full Page Translate and Partial Translate UIs.

// Note that the API offers a way to read/set language values through array
// indices. Such indices are only valid as long as the visual representation
// (infobar, bubble...) is in sync with the underlying language list which
// can actually change at run time (see translate_language_list.h).
// It is recommended that languages are only updated by language code to
// avoid bugs like crbug.com/555124
class TranslateUILanguagesManager {
 public:
  static const size_t kNoIndex = static_cast<size_t>(-1);

  TranslateUILanguagesManager(
      const base::WeakPtr<TranslateManager>& translate_manager,
      const std::vector<std::string>& language_codes,
      const std::string& source_language,
      const std::string& target_language);

  TranslateUILanguagesManager(const TranslateUILanguagesManager&) = delete;
  TranslateUILanguagesManager& operator=(const TranslateUILanguagesManager&) =
      delete;

  ~TranslateUILanguagesManager();

  // Returns the number of languages supported.
  size_t GetNumberOfLanguages() const;

  // Returns the initial source language index.
  size_t GetInitialSourceLanguageIndex() const {
    return initial_source_language_index_;
  }

  // Returns the source language index.
  size_t GetSourceLanguageIndex() const { return source_language_index_; }

  // Returns the source language code.
  std::string GetSourceLanguageCode() const;

  // Returns the target language index.
  size_t GetTargetLanguageIndex() const { return target_language_index_; }

  // Returns the target language code.
  std::string GetTargetLanguageCode() const;

  // Returns the ISO code for the language at |index|.
  std::string GetLanguageCodeAt(size_t index) const;

  // Returns the displayable name for the language at |index|.
  std::u16string GetLanguageNameAt(size_t index) const;

  // Updates the source language index.
  bool UpdateSourceLanguageIndex(size_t language_index);

  // Updates the source language and returns true if |language_code| exists in
  // the supported languages list. Returns false if it is not supported.
  bool UpdateSourceLanguage(const std::string& language_code);

  // If the language has changed, updates the target language index and returns
  // true. Returns false otherwise.
  bool UpdateTargetLanguageIndex(size_t language_index);

  // Updates the target language and returns true if |language_code| exists in
  // the supported languages list. Returns false if it is not supported.
  bool UpdateTargetLanguage(const std::string& language_code);

  static std::u16string GetUnknownLanguageDisplayName();

 private:
  // Returns a Collator object which helps to sort strings in a given locale or
  // null if unable to find the right collator.
  //
  // TODO(hajimehoshi): Write a test for icu::Collator::createInstance.
  std::unique_ptr<icu::Collator> CreateCollator(const std::string& locale);

  base::WeakPtr<TranslateManager> translate_manager_;

  // ISO code (en, fr...) -> displayable name in the current locale
  typedef std::pair<std::string, std::u16string> LanguageNamePair;

  // The list of supported languages for translation.
  // The languages are sorted alphabetically based on the displayable name.
  std::vector<LanguageNamePair> languages_;

  // The index of the language selected as the page's source language before a
  // translation.
  size_t source_language_index_;

  // The index of the language that the page is in when it was first reported
  // (source_language_index_ changes if the user selects a new source language,
  // but this does not).  This is necessary to report language detection errors
  // with the right source language even if the user changed the source
  // language.
  size_t initial_source_language_index_;

  // The index of the language selected as the target language for translation.
  size_t target_language_index_;

  // Translate related preferences.
  std::unique_ptr<TranslatePrefs> prefs_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_LANGUAGES_MANAGER_H_
