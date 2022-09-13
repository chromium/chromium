// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_INDEX_STRING_SPLITTER_H_
#define COMPONENTS_URL_PATTERN_INDEX_STRING_SPLITTER_H_

#include <iterator>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"

namespace url_pattern_index {

// A zero-allocation string splitter. Splits a string into non-empty tokens
// divided by separator characters as defined by the IsSeparator predicate.
// However, instead of materializing and returning a collection of all tokens in
// the string, it provides an InputIterator that can be used to extract the
// tokens.
//
// TODO(pkalinnikov): Move it to "base/strings" after some generalization.
template <typename IsSeparator>
class StringSplitter {
 public:
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = base::StringPiece;
    using difference_type = std::ptrdiff_t;
    using pointer = base::StringPiece*;
    using reference = base::StringPiece&;

    // Creates an iterator, which points to the leftmost token within the
    // |splitter|'s |text|, starting from |head|.
    Iterator(const StringSplitter& splitter,
             base::StringPiece::const_iterator head)
        : splitter_(&splitter), current_(head, 0), end_(splitter.text_.end()) {
      DCHECK_GE(head, splitter_->text_.begin());
      DCHECK_LE(head, end_);

      Advance();
    }

    bool operator==(const Iterator& rhs) const {
      return current_.begin() == rhs.current_.begin();
    }

    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

    base::StringPiece operator*() const { return current_; }
    const base::StringPiece* operator->() const { return &current_; }

    Iterator& operator++() {
      Advance();
      return *this;
    }

    Iterator operator++(int) {
      Iterator copy(*this);
      operator++();
      return copy;
    }

   private:
    void Advance() {
      base::StringPiece::const_iterator begin = current_.end();
      while (begin != end_ && splitter_->is_separator_(*begin))
        ++begin;
      base::StringPiece::const_iterator end = begin;
      while (end != end_ && !splitter_->is_separator_(*end))
        ++end;
      current_ = base::StringPiece(begin, end - begin);
    }

    raw_ptr<const StringSplitter<IsSeparator>> splitter_;

    // Contains the token currently pointed to by the iterator.
    base::StringPiece current_;
    // Always points to the text_.end().
    base::StringPiece::const_iterator end_;
  };

  // Constructs a splitter for iterating over non-empty tokens contained in the
  // |text|. |is_separator| predicate is used to determine whether a certain
  // character is a separator.
  StringSplitter(base::StringPiece text,
                 IsSeparator is_separator = IsSeparator())
      : text_(text), is_separator_(is_separator) {}

  Iterator begin() const { return Iterator(*this, text_.begin()); }
  Iterator end() const { return Iterator(*this, text_.end()); }

 private:
  base::StringPiece text_;
  IsSeparator is_separator_;
};

template <typename IsSeparator>
StringSplitter<IsSeparator> CreateStringSplitter(base::StringPiece text,
                                                 IsSeparator is_separator) {
  return StringSplitter<IsSeparator>(text, is_separator);
}

}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_STRING_SPLITTER_H_
