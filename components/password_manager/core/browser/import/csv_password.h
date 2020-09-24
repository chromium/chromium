// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_

#include <stddef.h>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// CSVPassword is a wrapper around one CSV line representing a credential.
// For more details, see
// https://docs.google.com/document/d/1wsZBl93S_WGaXZqrqq5SP08LVZ0zDKf6e9nlptyl9AY/edit?usp=sharing.
class CSVPassword {
 public:
  enum class Label { kOrigin, kUsername, kPassword };
  using ColumnMap = base::flat_map<size_t, Label>;

  // Status describes parsing errors.
  enum class Status { kOK, kSyntaxError, kSemanticError };

  // Number of values in the Label enum.
  static constexpr size_t kLabelCount = 3;

  explicit CSVPassword(const ColumnMap& map, base::StringPiece csv_row);
  CSVPassword(const CSVPassword&) = delete;
  CSVPassword(CSVPassword&&) = delete;
  CSVPassword& operator=(const CSVPassword&) = delete;
  CSVPassword& operator=(CSVPassword&&) = delete;
  ~CSVPassword();

  // Returns whether the associated CSV row can be parsed successfully. If
  // returning success, it also stores the parsed result in |*form|.
  Status Parse(PasswordForm* form) const;
  // TryParse() returns the same value as Parse(). However, TryParse() does not
  // attempt to create and store the corresponding PasswordForm anywhere.
  // Therefore TryParse() is faster than Parse() and a better choice for only
  // checking a correctness of a CSV serialization of a credential.
  Status TryParse() const;
  // Convenience wrapper around Parse() for cases known to be correctly
  // parseable.
  PasswordForm ParseValid() const;

 private:
  // ParseImpl is the common base of Parse() and TryParse().
  Status ParseImpl(PasswordForm* form) const;

  // The members |map_| and |row_| are only modified in constructor or
  // operator=().

  // |map_| stores the meaning of particular columns in the row.
  const ColumnMap& map_;
  // |row_| contains the CSV row from which the PasswordForm is parsed.
  base::StringPiece row_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_
