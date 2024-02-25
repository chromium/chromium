// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_CHAR_ITERATOR_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_CHAR_ITERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace base::i18n {
class UTF16CharIterator;
}

namespace ash::string_matching {

// An UTF16 char iterator for a TokenizedString.
class TokenizedStringCharIterator {
 public:
  struct State {
    State();
    State(size_t token_index, int char_index);

    size_t token_index;
    int32_t char_index;
  };

  // Requires |tokenized| out-lives this iterator.
  explicit TokenizedStringCharIterator(const TokenizedString& tokenized);

  TokenizedStringCharIterator(const TokenizedStringCharIterator&) = delete;
  TokenizedStringCharIterator& operator=(const TokenizedStringCharIterator&) =
      delete;

  ~TokenizedStringCharIterator();

  // Advances to the next char. Returns false if there is no next char.
  bool NextChar();

  // Advances to the first char of the next token. Returns false if there is
  // no next token.
  bool NextToken();

  // Returns the current char if there is one. Otherwise, returns 0.
  int32_t Get() const;

  // Returns the array index in original text of the tokenized string that is
  // passed in constructor.
  int32_t GetArrayPos() const;

  // Returns the number of UTF16 code units for the current char.
  size_t GetCharSize() const;

  // Returns true if the current char is the first char of the current token.
  bool IsFirstCharOfToken() const;

  // Returns true if the current char is the second char of the current token.
  bool IsSecondCharOfToken() const;

  // Helpers to get and restore the iterator's state.
  State GetState() const;
  void SetState(const State& state);

  // Returns true if the iterator is at the end.
  bool end() const { return !current_token_iter_; }

 private:
  void CreateTokenCharIterator();

  const raw_ref<const TokenizedString::Tokens> tokens_;
  const raw_ref<const TokenizedString::Mappings> mappings_;

  size_t current_token_;
  std::unique_ptr<base::i18n::UTF16CharIterator> current_token_iter_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_CHAR_ITERATOR_H_
