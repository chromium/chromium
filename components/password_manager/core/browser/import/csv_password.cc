// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

// Unescapes a CSV field |str|. |str| is assumed to exclude the outer pair of
// quotation marks, if originally present.
std::string UnescapeCSVField(std::string_view str) {
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

  CSVFieldParser parser(row);
  status_ = Status::kOK;

  for (size_t field_idx = 0; auto field = parser.NextField(); ++field_idx) {
    if (!field->has_value()) {
      status_ = Status::kSyntaxError;
      return;
    }
    auto meaning_it = map.find(field_idx);
    if (meaning_it == map.end()) {
      continue;
    }
    switch (meaning_it->second) {
      case Label::kOrigin: {
        GURL gurl = GURL(**field);
        if (!gurl.is_valid()) {
          url_ = base::unexpected(UnescapeCSVField(**field));
        } else {
          url_ = std::move(gurl);
        }
        break;
      }
      case Label::kUsername:
        username_ = UnescapeCSVField(**field);
        break;
      case Label::kPassword:
        password_ = UnescapeCSVField(**field);
        break;
      case Label::kNote:
        note_ = UnescapeCSVField(**field);
        break;
    }
  }
}

CSVPassword::CSVPassword(const CSVPassword&) = default;
CSVPassword::CSVPassword(CSVPassword&&) = default;
CSVPassword& CSVPassword::operator=(const CSVPassword&) = default;
CSVPassword& CSVPassword::operator=(CSVPassword&&) = default;
CSVPassword::~CSVPassword() = default;

}  // namespace password_manager
