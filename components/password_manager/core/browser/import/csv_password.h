// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_

#include <stddef.h>

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/types/expected.h"
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
  enum class Label { kOrigin, kUsername, kPassword, KNote };
  using ColumnMap = base::flat_map<size_t, Label>;

  // Status describes parsing errors.
  enum class Status {
    kOK = 0,
    kSyntaxError = 1,
    kSemanticError = 2,
  };

  explicit CSVPassword();
  explicit CSVPassword(const ColumnMap& map, std::string_view csv_row);
  explicit CSVPassword(GURL url,
                       std::string username,
                       std::string password,
                       std::string note,
                       Status status);
  // This constructor creates a valid CSVPassword but with an invalid_url, i.e.
  // the url is not a valid GURL.
  explicit CSVPassword(std::string invalid_url,
                       std::string username,
                       std::string password,
                       std::string note,
                       Status status);
  CSVPassword(const CSVPassword&);
  CSVPassword(CSVPassword&&);
  CSVPassword& operator=(const CSVPassword&);
  CSVPassword& operator=(CSVPassword&&);
  ~CSVPassword();

  // An exact equality comparison of all the fields - only used in tests.
  friend bool operator==(const CSVPassword&, const CSVPassword&) = default;

  Status GetParseStatus() const;

  const std::string& GetPassword() const;

  const std::string& GetUsername() const;

  const std::string& GetNote() const;

  // Returns the URL or the original raw url in case of an invalid GURL.
  const base::expected<GURL, std::string>& GetURL() const;

 private:
  // Contains a valid GURL or the original raw url in case of an invalid GURL.
  // Unparsed URL fields should also yield an emty URL.
  base::expected<GURL, std::string> url_ = base::unexpected("");
  std::string username_;
  std::string password_;
  std::string note_;

  Status status_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_PASSWORD_H_
