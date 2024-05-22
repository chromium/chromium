// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password.h"

#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

// ConvertUTF8() unescapes a CSV field |str| and converts the result to a 8-bit
// string. |str| is assumed to exclude the outer pair of quotation marks, if
// originally present.
std::string ConvertUTF8(std::string_view str) {
  std::string str_copy(str);
  base::ReplaceSubstringsAfterOffset(&str_copy, 0, "\"\"", "\"");
  return str_copy;
}

}  // namespace

CSVPassword::CSVPassword() : status_(Status::kSemanticError) {}

CSVPassword::CSVPassword(GURL url,
                         std::string username,
                         std::string password,
                         std::string note,
                         Status status)
    : url_(std::move(url)),
      username_(std::move(username)),
      password_(std::move(password)),
      note_(std::move(note)),
      status_(status) {}

CSVPassword::CSVPassword(std::string invalid_url,
                         std::string username,
                         std::string password,
                         std::string note,
                         Status status)
    : url_(base::unexpected(std::move(invalid_url))),
      username_(std::move(username)),
      password_(std::move(password)),
      note_(std::move(note)),
      status_(status) {}

CSVPassword::CSVPassword(const ColumnMap& map, std::string_view row) {
  if (row.empty()) {
    status_ = Status::kSemanticError;
    return;
  }

  size_t field_idx = 0;
  CSVFieldParser parser(row);
  status_ = Status::kOK;

  while (parser.HasMoreFields()) {
    std::string_view field;
    if (!parser.NextField(&field)) {
      status_ = Status::kSyntaxError;
      return;
    }
    auto meaning_it = map.find(field_idx++);
    if (meaning_it == map.end()) {
      continue;
    }
    switch (meaning_it->second) {
      case Label::kOrigin: {
        GURL gurl = GURL(field);
        if (!gurl.is_valid()) {
          url_ = base::unexpected(ConvertUTF8(field));
        } else {
          url_ = gurl;
        }
        break;
      }
      case Label::kUsername:
        username_ = ConvertUTF8(field);
        break;
      case Label::kPassword:
        password_ = ConvertUTF8(field);
        break;
      case Label::KNote:
        note_ = ConvertUTF8(field);
        break;
    }
  }
}

CSVPassword::CSVPassword(const CSVPassword&) = default;
CSVPassword::CSVPassword(CSVPassword&&) = default;
CSVPassword& CSVPassword::operator=(const CSVPassword&) = default;
CSVPassword& CSVPassword::operator=(CSVPassword&&) = default;
CSVPassword::~CSVPassword() = default;

CSVPassword::Status CSVPassword::GetParseStatus() const {
  return status_;
}

const std::string& CSVPassword::GetPassword() const {
  return password_;
}

const std::string& CSVPassword::GetUsername() const {
  return username_;
}

const std::string& CSVPassword::GetNote() const {
  return note_;
}

const base::expected<GURL, std::string>& CSVPassword::GetURL() const {
  return url_;
}

}  // namespace password_manager
