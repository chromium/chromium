// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_CUSTOM_DICTIONARY_ENGINE_H_
#define COMPONENTS_SPELLCHECK_RENDERER_CUSTOM_DICTIONARY_ENGINE_H_

#include <set>
#include <string>

// Custom spellcheck dictionary. Words in this dictionary are always correctly
// spelled. Words that are not in this dictionary may or may not be correctly
// spelled.
class CustomDictionaryEngine {
 public:
  CustomDictionaryEngine();

  CustomDictionaryEngine(const CustomDictionaryEngine&) = delete;
  CustomDictionaryEngine& operator=(const CustomDictionaryEngine&) = delete;

  ~CustomDictionaryEngine();

  // Initialize the custom dictionary engine.
  void Init(const std::set<std::string>& words);

  // Spellcheck |text|. Assumes that another spelling engine has set
  // |misspelling_start| and |misspelling_len| to indicate a misspelling.
  // Returns true if there are no misspellings, otherwise returns false.
  bool SpellCheckWord(const std::u16string& text,
                      size_t misspelling_start,
                      size_t misspelling_len);

  // Update custom dictionary words.
  void OnCustomDictionaryChanged(const std::set<std::string>& words_added,
                                 const std::set<std::string>& words_removed);

 private:
  // Correctly spelled words.
  std::set<std::u16string> dictionary_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_CUSTOM_DICTIONARY_ENGINE_H_
