// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_CSV_READER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_CSV_READER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace password_manager {

// Parsed representation of tabular CSV data.
class CSVTable {
 public:
  CSVTable();
  ~CSVTable();

  // Reads tabular data |csv| in a CSV (Comma Separated Values) format and fills
  // the column_names_ and records_ accordingly. The CSV format is understood as
  // defined in RFC 4180, with the following limitations/relaxations:
  //   * The input should be UTF-8 encoded. No code points should be escaped.
  //   * The first line must be a header that contains the column names.
  //   * Records may be separated by either LF or CRLF sequences.
  //   * Inconsistent number of fields within records is handled gracefully.
  //     Extra fields are ignored. Missing fields will have no corresponding
  //     key-value pair in the record.
  //   * Seeing a row with too many cells is a syntax error (see CSVFieldParser
  //     for the actual limit).
  //   * Repeated columns of the same name are not supported (the last value
  //     will be preserved).
  // Returns false if parsing failed due to a syntax error.
  bool ReadCSV(base::StringPiece csv);

  const std::vector<std::string>& column_names() const { return column_names_; }

  const std::vector<std::map<base::StringPiece, std::string>>& records() const {
    return records_;
  }

 private:
  // Values from the first row.
  std::vector<std::string> column_names_;
  // Values from the subsequent rows. Each map represents one row and maps the
  // column names to the value stored at that column in that row.
  std::vector<std::map<base::StringPiece, std::string>> records_;

  DISALLOW_COPY_AND_ASSIGN(CSVTable);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_CSV_READER_H_
