// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inline_autocompletion_util.h"

size_t FindAtWordbreak(const std::u16string& text,
                       const std::u16string& search,
                       size_t search_start) {
  std::vector<size_t> word_starts;
  String16VectorFromString16(text, false, &word_starts);
  size_t next_occurrence = std::string::npos;
  for (auto word_start : word_starts) {
    if (word_start < search_start)
      continue;
    if (next_occurrence != std::string::npos && word_start < next_occurrence)
      continue;
    next_occurrence = text.find(search, word_start);
    if (next_occurrence == std::string::npos)
      break;
    if (word_start == next_occurrence)
      return next_occurrence;
  }
  return std::string::npos;
}

std::vector<std::pair<size_t, size_t>> FindWordsSequentiallyAtWordbreak(
    const std::u16string& text,
    const std::u16string& search) {
  std::vector<std::pair<size_t, size_t>> occurrences;
  size_t cursor = 0u;
  std::vector<size_t> search_word_starts{};
  auto search_words =
      String16VectorFromString16(search, false, &search_word_starts);
  for (size_t i = 0; i < search_word_starts.size(); ++i) {
    auto search_word = search_words[i];
    // The non-word characters following |search_word|. Can be empty for the
    // last word. Can be multiple characters.
    auto delimiter =
        search
            .substr(search_word_starts[i],
                    i == search_word_starts.size() - 1
                        ? std::u16string::npos
                        : search_word_starts[i + 1] - search_word_starts[i])
            .substr(search_word.size());
    if ((cursor = FindAtWordbreak(text, search_word, cursor)) ==
        std::string::npos)
      return {};
    occurrences.emplace_back(cursor, cursor + search_word.size());
    cursor += search_word.size();
    if (delimiter.empty())
      continue;
    if ((cursor = text.find(delimiter, cursor)) == std::string::npos)
      return {};
    occurrences.emplace_back(cursor, cursor + delimiter.size());
    cursor += delimiter.size();
  }
  return occurrences;
}

std::vector<gfx::Range> TermMatchesToSelections(
    size_t length,
    std::vector<std::pair<size_t, size_t>> matches) {
  std::vector<gfx::Range> selections;
  size_t cursor = length;
  for (size_t i = matches.size(); i-- != 0;) {
    auto match = matches[i];
    // Include the 1st iterated match as is. The 1st match determines the user's
    // cursor, and skipping or merging the match would incorrectly move the
    // user's cursor.
    if (i == matches.size() - 1) {
      selections.emplace_back(cursor, match.second);
      cursor = match.first;
      continue;
    }
    // Skip empty matches.
    if (match.first == match.second)
      continue;
    // Merge adjacent matches.
    if (cursor != match.second)
      selections.emplace_back(cursor, match.second);
    cursor = match.first;
  }
  if (cursor != 0)
    selections.emplace_back(cursor, 0);
  return selections;
}
