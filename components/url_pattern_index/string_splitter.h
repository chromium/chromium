// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_URL_PATTERN_INDEX_STRING_SPLITTER_H_
#define COMPONENTS_URL_PATTERN_INDEX_STRING_SPLITTER_H_

#include <iterator>
#include <string_view>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"

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
    using value_type = std::string_view;
    using difference_type = std::ptrdiff_t;
    using pointer = std::string_view*;
    using reference = std::string_view&;

    bool operator==(const Iterator& rhs) const {
      DCHECK_EQ(splitter_, rhs.splitter_);
      // If `current_` starts at the same position, all the other locations will
      // match.
      return current_.data() == rhs.current_.data();
    }

    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

    std::string_view operator*() const { return current_; }
    const std::string_view* operator->() const { return &current_; }

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
    friend class StringSplitter<IsSeparator>;

    // Creates an iterator, which points to the leftmost token within
    // `remaining`, which must be a suffix of `splitter`'s `text`.
    Iterator(const StringSplitter& splitter, std::string_view remaining)
        : splitter_(&splitter), remaining_(remaining) {
      DCHECK_LE(splitter_->text_.data(), remaining_.data());
      DCHECK_EQ(splitter_->text_.data() + splitter_->text_.size(),
                remaining_.data() + remaining_.size());
      Advance();
    }

    void Advance() {
      std::string_view::const_iterator begin = remaining_.begin();
      while (begin != remaining_.end() && splitter_->is_separator_(*begin)) {
        ++begin;
      }
      std::string_view::const_iterator end = begin;
      while (end != remaining_.end() && !splitter_->is_separator_(*end)) {
        ++end;
      }
      current_ = std::string_view(begin, end);
      remaining_ = std::string_view(end, remaining_.end());
    }

    raw_ptr<const StringSplitter<IsSeparator>> splitter_;

    // Contains the token currently pointed to by the iterator.
    std::string_view current_;
    // Contains the remaining text, starting from the current token and ending
    // at `text_.end()`.
    std::string_view remaining_;
  };

  // Constructs a splitter for iterating over non-empty tokens contained in the
  // `text`. `is_separator` predicate is used to determine whether a certain
  // character is a separator.
  explicit StringSplitter(std::string_view text,
                          IsSeparator is_separator = IsSeparator())
      : text_(text), is_separator_(is_separator) {}

  Iterator begin() const { return Iterator(*this, text_); }
  Iterator end() const { return Iterator(*this, text_.substr(text_.size())); }

 private:
  std::string_view text_;
  IsSeparator is_separator_;
};

template <typename IsSeparator>
StringSplitter<IsSeparator> CreateStringSplitter(std::string_view text,
                                                 IsSeparator is_separator) {
  return StringSplitter<IsSeparator>(text, is_separator);
}

}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_STRING_SPLITTER_H_
