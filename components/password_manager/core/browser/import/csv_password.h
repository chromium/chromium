// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_

#include <stddef.h>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "url/gurl.h"

namespace password_manager {

// CSVPassword is a wrapper around one CSV line representing a credential.
// For more details, see
// https://docs.google.com/document/d/1wsZBl93S_WGaXZqrqq5SP08LVZ0zDKf6e9nlptyl9AY/edit?usp=sharing.
// CSVPassword contains a triple (url, password, username).
// In case of a valid URL, a GURL is provided, otherwise the original raw URL.
// Partial parsing i.e. missing fields will also yield a valid CSVPassword.
class CSVPassword {
 public:
  enum class Label { kOrigin, kUsername, kPassword };
  using ColumnMap = base::flat_map<size_t, Label>;

  // Status describes parsing errors.
  enum class Status {
    kOK = 0,
    kSyntaxError = 1,
    kSemanticError = 2,
  };

  // Number of values in the Label enum.
  static constexpr size_t kLabelCount = 3;

  explicit CSVPassword();
  explicit CSVPassword(const ColumnMap& map, base::StringPiece csv_row);
  explicit CSVPassword(GURL url,
                       std::string username,
                       std::string password,
                       Status status);
  // This constructor creates a valid CSVPassword but with an invalid_url, i.e.
  // the url is not a valid GURL.
  explicit CSVPassword(std::string invalid_url,
                       std::string username,
                       std::string password,
                       Status status);
  CSVPassword(const CSVPassword&);
  CSVPassword(CSVPassword&&);
  CSVPassword& operator=(const CSVPassword&);
  CSVPassword& operator=(CSVPassword&&);
  ~CSVPassword();

  // Returns the status of the parse.
  Status GetParseStatus() const;

  // Returns the password.
  const std::string& GetPassword() const;

  // Returns the username.
  const std::string& GetUsername() const;

  // Returns the URL or the original raw url in case of an invalid GURL.
  const base::expected<GURL, std::string>& GetURL() const;

 private:
  // Contains a valid GURL or the original raw url in case of an invalid GURL.
  // Unparsed URL fields should also yield an emty URL.
  base::expected<GURL, std::string> url_ = base::unexpected("");
  std::string username_;
  std::string password_;

  Status status_;
};

// An exact equality comparison of all the fields is only used for tests.
bool operator==(const CSVPassword& lhs, const CSVPassword& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_
