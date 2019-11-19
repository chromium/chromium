// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_SEQUENCE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_SEQUENCE_H_

#include <string>

#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_iterator.h"

namespace password_manager {

// CSVPasswordSequence takes a string with CSV representation of multiple
// credentials and exposes the corresponding PasswordForms in a sequence,
// parsing as necessary. For more details, see
// https://docs.google.com/document/d/1wsZBl93S_WGaXZqrqq5SP08LVZ0zDKf6e9nlptyl9AY/edit?usp=sharing.
class CSVPasswordSequence {
 public:
  // Construct a CSVPasswordSequence over the provided CSV description |csv| of
  // the credentials (one credential per row, with the initial row describing
  // the columns). |csv| is not modified by CSVPasswordSequence, so it could be
  // a StringView if multiple CSVPasswordSequences needed to share one CSV
  // description. However, that sharing scenario is unlikely, and passing a
  // std::string avoids the danger of accessing the CSV string from
  // CSVPasswordSequence after the CSV string has been deleted.
  explicit CSVPasswordSequence(std::string csv);

  // While the copy constructor and operator= could be defined, because there is
  // nothing in the design of CSVPasswordSequence preventing cloning it, it does
  // not seem necessary in the current code to have these, so they are deleted
  // to keep the code and API simpler.
  CSVPasswordSequence(const CSVPasswordSequence&) = delete;
  CSVPasswordSequence& operator=(const CSVPasswordSequence&) = delete;

  CSVPasswordSequence(CSVPasswordSequence&&);
  CSVPasswordSequence& operator=(CSVPasswordSequence&&);

  ~CSVPasswordSequence();

  // Returns an iterator corresponding to the second CSV row. If there is no
  // second CSV row or if errors were detected, then returns the same iterator
  // as end() below.
  CSVPasswordIterator begin() const;
  // Returns an iterator pointing just behind the end of the CSV data.
  CSVPasswordIterator end() const;

  CSVPassword::Status result() const { return result_; }

 private:
  // All data members are only modified in constructors. They cannot be marked
  // const, because the move constructor moves them out of the moved object.

  // |csv_| keeps the CSV input which is parsed to provide the PasswordForms.
  std::string csv_;
  // |map_| stores the meaning of particular columns in the row.
  CSVPassword::ColumnMap map_;
  // |data_rows_| captures all but the first row of the parsed CSV string.
  // These are the rows containing the credentials, as opposed to the first
  // row containing the column names.
  base::StringPiece data_rows_;
  // |result_| captures whether the CSV contains correctly CSV-encoded
  // credentials.
  CSVPassword::Status result_ = CSVPassword::Status::kOK;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_SEQUENCE_H_
