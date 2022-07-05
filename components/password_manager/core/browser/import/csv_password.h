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

  explicit CSVPassword();
  explicit CSVPassword(const ColumnMap& map, base::StringPiece csv_row);
  explicit CSVPassword(GURL url, std::string username, std::string password);
  CSVPassword(const CSVPassword&);
  CSVPassword(CSVPassword&&);
  CSVPassword& operator=(const CSVPassword&);
  CSVPassword& operator=(CSVPassword&&);

  // Returns the status of the parse.
  Status GetParseStatus() const;

  // Returns the password.
  const std::string& GetPassword() const;

  // Returns the username.
  const std::string& GetUsername() const;

  // Returns the URL.
  const GURL& GetURL() const;

  // Returns PasswordForm populated with parsed data, if initial parsing
  // completed successfully.
  PasswordForm ToPasswordForm() const;

 private:
  GURL url_;
  std::string username_;
  std::string password_;

  Status status_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_
