// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_field_parser.h"

#include <stddef.h>

#include <optional>
#include <string_view>
#include <utility>

#include "base/types/expected.h"

namespace password_manager {

CSVFieldParser::CSVFieldParser(std::string_view row) : rest_(row) {}

CSVFieldParser::~CSVFieldParser() = default;

std::optional<base::expected<std::string_view, CSVFieldParser::SyntaxError>>
CSVFieldParser::NextField() {
  std::optional<std::string_view> row = std::exchange(rest_, std::nullopt);
  if (!row.has_value()) {
    return std::nullopt;
  }

  if (fields_returned_ >= kMaxFields) {
    return base::unexpected(SyntaxError());
  }
  ++fields_returned_;

  if (!row->starts_with('"')) {
    size_t comma = row->find(',');
    if (comma == std::string_view::npos) {
      return *row;
    }
    rest_ = row->substr(comma + 1);
    return row->substr(0, comma);
  }

  std::string_view quoted = row->substr(1);
  bool in_quotes = true;
  for (size_t i = 0; i < quoted.size(); ++i) {
    char read = quoted[i];
    if (in_quotes) {
      if (read == '"') {
        in_quotes = false;
      }
    } else if (read == ',') {
      rest_ = quoted.substr(i + 1);
      quoted = quoted.substr(0, i);
      break;
    } else if (read == '"') {
      in_quotes = true;
    } else {
      return base::unexpected(SyntaxError());
    }
  }

  if (in_quotes) {
    return base::unexpected(SyntaxError());
  }
  return quoted.substr(0, quoted.size() - 1);
}

}  // namespace password_manager
