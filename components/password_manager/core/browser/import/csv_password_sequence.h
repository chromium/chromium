// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_SEQUENCE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_SEQUENCE_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"

namespace password_manager {

// CSVPasswordSequence takes a string with CSV representation of multiple
// credentials and yields the corresponding CSVPasswords in a sequence,
// parsing as necessary. For more details, see
// https://docs.google.com/document/d/1wsZBl93S_WGaXZqrqq5SP08LVZ0zDKf6e9nlptyl9AY/edit?usp=sharing.
class CSVPasswordSequence {
 public:
  // Constructs a CSVPasswordSequence over the provided CSV description |csv| of
  // the credentials (one credential per row, with the initial row describing
  // the columns), or returns the header parse error status.
  static base::expected<CSVPasswordSequence, CSVPassword::Status> Create(
      std::string_view csv LIFETIME_BOUND);

  CSVPasswordSequence(const CSVPasswordSequence&) = delete;
  CSVPasswordSequence& operator=(const CSVPasswordSequence&) = delete;

  CSVPasswordSequence(CSVPasswordSequence&&);
  CSVPasswordSequence& operator=(CSVPasswordSequence&&);

  ~CSVPasswordSequence();

  // Parses and returns the next non-empty credential row, or std::nullopt when
  // the end of the CSV data is reached.
  std::optional<CSVPassword> NextPassword();

 private:
  struct ColumnMap {
    size_t origin = 0;
    size_t username = 0;
    size_t password = 0;
    std::optional<size_t> note;
  };

  CSVPasswordSequence(ColumnMap column_map,
                      std::string_view remaining_rows LIFETIME_BOUND);

  // Stores the meaning of particular columns in the row.
  ColumnMap column_map_;
  // Captures the remaining unconsumed rows of the parsed CSV string.
  std::string_view remaining_rows_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_SEQUENCE_H_
