// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_INDEX_NGRAM_EXTRACTOR_H_
#define COMPONENTS_URL_PATTERN_INDEX_NGRAM_EXTRACTOR_H_

#include <stddef.h>

#include <iterator>
#include <string_view>
#include <type_traits>

#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_util.h"

namespace url_pattern_index {

// The class used to iteratively extract N-grams from strings. An N-gram is a
// string consisting of N (up to 8) non-special characters, which are stored in
// the lowest N non-zero bytes, lower bytes corresponding to later symbols. The
// size of the integer type limits the maximum value of N. For example an
// uint64_t can store up to 8-grams.
//
// Note: If used for UTF-8 strings, the N-grams can have partial byte sequences.
//
// Template parameters:
//  * N - the size of N-grams.
//  * NGramType - the integer type used to encode N-grams.
//  * CasePolicy - whether or not to lower-case the N-grams. Assumes ASCII.
//  * IsSeparator - the type of a bool(char) functor.
enum class NGramCaseExtraction { kCaseSensitive, kLowerCase };
template <size_t N,
          typename NGramType,
          NGramCaseExtraction CasePolicy,
          typename IsSeparator>
class NGramExtractor {
 public:
  // An STL compatible input iterator over N-grams contained in a string.
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = NGramType;
    using difference_type = std::ptrdiff_t;
    using pointer = NGramType*;
    using reference = NGramType&;

    // Creates an iterator, which points to the leftmost valid N-gram within the
    // |extractor|'s string, starting from |head|.
    Iterator(const NGramExtractor& extractor,
             std::string_view::const_iterator head)
        : extractor_(extractor), head_(head), end_(extractor.string_.end()) {
      DCHECK(head >= extractor_->string_.begin());
      DCHECK(head <= end_);

      CompleteNGramFrom(0);
    }

    bool operator==(const Iterator& rhs) const { return head_ == rhs.head_; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

    NGramType operator*() const { return ngram_; }
    NGramType* operator->() const { return &ngram_; }

    Iterator& operator++() {
      ngram_ &= ~(static_cast<NGramType>(0xFFu) << 8 * (N - 1));
      ++head_;
      CompleteNGramFrom(N - 1);
      return *this;
    }

    Iterator operator++(int) {
      Iterator copy(*this);
      operator++();
      return copy;
    }

   private:
    char ExtractHeadByte() {
      char head_byte = *head_;
      switch (CasePolicy) {
        case NGramCaseExtraction::kCaseSensitive:
          return head_byte;
        case NGramCaseExtraction::kLowerCase:
          return base::ToLowerASCII(head_byte);
      }
    }
    // Consumes characters starting with the one pointed to by |head_|, as many
    // of them as needed to extend |ngram_| from its |current_length| to a
    // length of N. Leaves |head_| pointing to the last character consumed.
    void CompleteNGramFrom(size_t current_length) {
      for (; head_ != end_; ++head_) {
        if (extractor_->is_separator_(*head_)) {
          current_length = 0;
          ngram_ = 0;
        } else {
          ngram_ = ngram_ << 8 | static_cast<NGramType>(ExtractHeadByte());
          if (++current_length == N)
            break;
        }
      }
    }

    const raw_ref<const NGramExtractor> extractor_;

    // Always points to the last character included in the current |ngram_|.
    std::string_view::const_iterator head_;
    // Always points to extractor_.string_.end().
    std::string_view::const_iterator end_;

    // Contains the N-gram currently pointed to by the iterator. Undefined if
    // the iterator is at the end.
    NGramType ngram_ = 0;
  };

  // Constructs an extractor for iterating over N-grams contained in the
  // |string|. |is_separator| is used to determine whether a certain character
  // is a separator and should not be contained in an N-gram.
  NGramExtractor(std::string_view string, IsSeparator is_separator)
      : string_(string), is_separator_(is_separator) {}

  Iterator begin() const { return Iterator(*this, string_.begin()); }
  Iterator end() const { return Iterator(*this, string_.end()); }

 private:
  static_assert(std::is_integral<NGramType>::value, "Not an integral type.");
  static_assert(std::is_unsigned<NGramType>::value, "Not an unsigned type.");
  static_assert(N > 0u, "N should be positive.");
  static_assert(N <= sizeof(NGramType), "N-gram doesn't fit into the type.");

  std::string_view string_;
  IsSeparator is_separator_;
};

// A helper function used to create an NGramExtractor for a |string| without
// knowing the direct type of the |is_separator| functor.
//
// Typical usage:
//   const char* str = "no*abacaba*abcd";
//   auto extractor =
//     CreateNGramExtractor<5, uint64_t, NGrameCaseExtraction::kLowercase>(
//       str, [](char c) { return c == '*'; });
//   for (uint64_t ngram : extractor) {
//     ... process the |ngram| ...
//   }
template <size_t N,
          typename NGramType,
          NGramCaseExtraction CasePolicy,
          typename IsSeparator>
NGramExtractor<N, NGramType, CasePolicy, IsSeparator> CreateNGramExtractor(
    std::string_view string,
    IsSeparator is_separator) {
  return NGramExtractor<N, NGramType, CasePolicy, IsSeparator>(string,
                                                               is_separator);
}

}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_NGRAM_EXTRACTOR_H_
