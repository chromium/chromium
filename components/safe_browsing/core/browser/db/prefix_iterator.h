// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_PREFIX_ITERATOR_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_PREFIX_ITERATOR_H_

#include <cstddef>
#include <iterator>

#include "base/strings/string_piece.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

namespace safe_browsing {

// The prefix iterator is used to binary search within a |HashPrefixes|. It is
// essentially a random access iterator that steps |PrefixSize| steps within the
// underlying buffer.
class PrefixIterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = base::StringPiece;
  using difference_type = std::ptrdiff_t;
  using pointer = base::StringPiece*;
  using reference = base::StringPiece&;

  PrefixIterator(base::StringPiece prefixes, size_t index, size_t size);
  PrefixIterator(const PrefixIterator& rhs);

  base::StringPiece operator*() const { return GetPiece(index_); }
  base::StringPiece operator[](const int& rhs) const {
    return GetPiece(index_ + rhs);
  }

  PrefixIterator& operator=(const PrefixIterator& rhs) {
    index_ = rhs.index_;
    return *this;
  }
  PrefixIterator& operator+=(const int& rhs) {
    index_ += rhs;
    return *this;
  }
  PrefixIterator& operator-=(const int& rhs) {
    index_ -= rhs;
    return *this;
  }
  PrefixIterator& operator++() {
    index_++;
    return *this;
  }
  PrefixIterator& operator--() {
    index_--;
    return *this;
  }

  PrefixIterator operator+(const PrefixIterator& rhs) const {
    return PrefixIterator(prefixes_, index_ + rhs.index_, size_);
  }
  difference_type operator-(const PrefixIterator& rhs) const {
    return index_ - rhs.index_;
  }
  PrefixIterator operator+(const int& rhs) const {
    return PrefixIterator(prefixes_, index_ + rhs, size_);
  }
  PrefixIterator operator-(const int& rhs) const {
    return PrefixIterator(prefixes_, index_ - rhs, size_);
  }

  bool operator==(const PrefixIterator& rhs) const {
    return index_ == rhs.index_;
  }
  bool operator!=(const PrefixIterator& rhs) const {
    return index_ != rhs.index_;
  }
  bool operator>(const PrefixIterator& rhs) const {
    return index_ > rhs.index_;
  }
  bool operator<(const PrefixIterator& rhs) const {
    return index_ < rhs.index_;
  }
  bool operator>=(const PrefixIterator& rhs) const {
    return index_ >= rhs.index_;
  }
  bool operator<=(const PrefixIterator& rhs) const {
    return index_ <= rhs.index_;
  }

 private:
  base::StringPiece GetPiece(size_t index) const {
    return prefixes_.substr(index * size_, size_);
  }

  base::StringPiece prefixes_;
  size_t index_;
  size_t size_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_PREFIX_ITERATOR_H_
