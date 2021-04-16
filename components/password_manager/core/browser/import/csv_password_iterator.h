// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_ITERATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_ITERATOR_H_

#include <stddef.h>

#include <iterator>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "components/password_manager/core/browser/import/csv_password.h"

namespace password_manager {

// CSVPasswordIterator abstracts reading a CSV text line by line by creating
// and providing a CSVPassword for each line. For more details, see
// https://docs.google.com/document/d/1wsZBl93S_WGaXZqrqq5SP08LVZ0zDKf6e9nlptyl9AY/edit?usp=sharing.
class CSVPasswordIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = CSVPassword;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type*;
  using reference = const value_type&;

  CSVPasswordIterator();
  explicit CSVPasswordIterator(const CSVPassword::ColumnMap& map,
                               base::StringPiece csv);
  CSVPasswordIterator(const CSVPasswordIterator&);
  CSVPasswordIterator& operator=(const CSVPasswordIterator&);
  ~CSVPasswordIterator();

  reference operator*() const {
    DCHECK(password_);
    return *password_;
  }
  pointer operator->() const { return &**this; }

  CSVPasswordIterator& operator++();
  CSVPasswordIterator operator++(int);

  // Defining comparison as methods rather than non-member functions, because
  // while the latter allows implicit conversions on the lhs argument as well,
  // there are no implicit conversions available for CSVPasswordIterator, and
  // the methods avoid having to declare the operators as friends.
  bool operator==(const CSVPasswordIterator& other) const;
  bool operator!=(const CSVPasswordIterator& other) const {
    return !(*this == other);
  }

 private:
  // SeekToNextValidRow seeks the iterator to the first available data row which
  // is valid, or to the end of the CSV if there is no such row.
  void SeekToNextValidRow();

  // |map_| stores the meaning of particular columns in the row.
  const CSVPassword::ColumnMap* map_ = nullptr;
  // |csv_rest_| contains the CSV lines left to be iterated over.
  base::StringPiece csv_rest_;
  // |csv_row_| contains the CSV row which the iterator points at.
  base::StringPiece csv_row_;
  // Contains a CSVPassword created from |map_| and |csv_row_| if possible.
  base::Optional<CSVPassword> password_;
};

// ConsumeCSVLine is a shared utility between CSVPasswordIterator (which uses
// it to consume data rows) and CSVPasswordSequence (which uses it to consume
// the header row). Therefore it is public, but it is not intended for use
// beyond those two classes.
// ConsumeCSVLine returns all the characters from the start of |input| until
// the first '\n', "\r\n" (exclusive) or the end of |input|. Cuts the returned
// part (inclusive the line breaks) from |input|. Skips blocks of matching
// quotes. Examples:
// old input -> returned value, new input
// "ab\ncd" -> "ab", "cd"
// "\r\n" -> "", ""
// "abcd" -> "abcd", ""
// "\r" -> "\r", ""
// "a\"\n\"b" -> "a\"\n\"b", ""
base::StringPiece ConsumeCSVLine(base::StringPiece* input);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_ITERATOR_H_
