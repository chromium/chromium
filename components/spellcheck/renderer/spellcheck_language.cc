// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_language.h"

#include <utility>

#include "base/logging.h"
#include "components/spellcheck/renderer/spellcheck_worditerator.h"
#include "components/spellcheck/renderer/spelling_engine.h"

SpellcheckLanguage::SpellcheckLanguage(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : platform_spelling_engine_(CreateNativeSpellingEngine(embedder_provider)) {
}

SpellcheckLanguage::~SpellcheckLanguage() {
}

void SpellcheckLanguage::Init(base::File file, const std::string& language) {
  DCHECK(platform_spelling_engine_);
  platform_spelling_engine_->Init(std::move(file));

  character_attributes_.SetDefaultLanguage(language);
  text_iterator_.Reset();
  contraction_iterator_.Reset();
}

bool SpellcheckLanguage::InitializeIfNeeded() {
  DCHECK(platform_spelling_engine_);
  return platform_spelling_engine_->InitializeIfNeeded();
}

SpellcheckLanguage::SpellcheckWordResult SpellcheckLanguage::SpellCheckWord(
    const char16_t* text_begin,
    size_t position_in_text,
    size_t text_length,
    int tag,
    size_t* skip_or_misspelling_start,
    size_t* skip_or_misspelling_len,
    std::vector<std::u16string>* optional_suggestions) {
  DCHECK_GE(text_length, position_in_text);
  DCHECK(skip_or_misspelling_start && skip_or_misspelling_len)
      << "Out vars must be given.";
  size_t remaining_text_len = text_length - position_in_text;

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return IS_CORRECT;

  // Do nothing if spell checking is disabled.
  if (!platform_spelling_engine_ || !platform_spelling_engine_->IsEnabled())
    return IS_CORRECT;

  *skip_or_misspelling_start = 0;
  *skip_or_misspelling_len = 0;
  if (remaining_text_len == 0)
    return IS_CORRECT;  // No input means always spelled correctly.

  std::u16string word;
  size_t word_start;
  size_t word_length;
  if (!text_iterator_.IsInitialized() &&
      !text_iterator_.Initialize(&character_attributes_, true)) {
      // We failed to initialize text_iterator_, return as spelled correctly.
      VLOG(1) << "Failed to initialize SpellcheckWordIterator";
      return IS_CORRECT;
  }

  text_iterator_.SetText(text_begin + position_in_text, remaining_text_len);
  DCHECK(platform_spelling_engine_);
  for (SpellcheckWordIterator::WordIteratorStatus status =
           text_iterator_.GetNextWord(&word, &word_start, &word_length);
       status != SpellcheckWordIterator::IS_END_OF_TEXT;
       status = text_iterator_.GetNextWord(&word, &word_start, &word_length)) {
    // Found a character that is not able to be spellchecked so determine how
    // long the sequence of uncheckable characters is and then return.
    if (status == SpellcheckWordIterator::IS_SKIPPABLE) {
      *skip_or_misspelling_start = position_in_text + word_start;
      while (status == SpellcheckWordIterator::IS_SKIPPABLE) {
        *skip_or_misspelling_len += word_length;
        status = text_iterator_.GetNextWord(&word, &word_start, &word_length);
      }
      return IS_SKIPPABLE;
    }

    // Found a word (or a contraction) that the spellchecker can check the
    // spelling of.
    if (platform_spelling_engine_->CheckSpelling(word, tag))
      continue;

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word, tag))
      continue;

    *skip_or_misspelling_start = position_in_text + word_start;
    *skip_or_misspelling_len = word_length;

    // Get the list of suggested words.
    if (optional_suggestions) {
      platform_spelling_engine_->FillSuggestionList(word,
                                                    optional_suggestions);
    }
    return IS_MISSPELLED;
  }

  return IS_CORRECT;
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellcheckLanguage::IsValidContraction(const std::u16string& contraction,
                                            int tag) {
  if (!contraction_iterator_.IsInitialized() &&
      !contraction_iterator_.Initialize(&character_attributes_, false)) {
    // We failed to initialize the word iterator, return as spelled correctly.
    VLOG(1) << "Failed to initialize contraction_iterator_";
    return true;
  }

  contraction_iterator_.SetText(contraction.c_str(), contraction.length());

  std::u16string word;
  size_t word_start;
  size_t word_length;

  DCHECK(platform_spelling_engine_);
  for (SpellcheckWordIterator::WordIteratorStatus status =
           contraction_iterator_.GetNextWord(&word, &word_start, &word_length);
       status != SpellcheckWordIterator::IS_END_OF_TEXT;
       status = contraction_iterator_.GetNextWord(&word, &word_start,
                                                  &word_length)) {
    if (status == SpellcheckWordIterator::IS_SKIPPABLE)
      continue;

    if (!platform_spelling_engine_->CheckSpelling(word, tag))
      return false;
  }
  return true;
}

bool SpellcheckLanguage::IsEnabled() {
  DCHECK(platform_spelling_engine_);
  return platform_spelling_engine_->IsEnabled();
}

bool SpellcheckLanguage::IsTextInSameScript(const std::u16string& text) const {
  return character_attributes_.IsTextInSameScript(text);
}
