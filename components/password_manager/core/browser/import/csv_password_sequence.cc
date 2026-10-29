// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_sequence.h"

#include <stddef.h>

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

enum class Label {
  kOrigin,
  kUsername,
  kPassword,
};

// Unescapes a CSV field |str|. |str| is assumed to exclude the outer pair of
// quotation marks, if originally present.
std::string UnescapeCSVField(std::string_view str) {
  std::string str_copy(str);
  base::ReplaceSubstringsAfterOffset(&str_copy, 0, "\"\"", "\"");
  return str_copy;
}

// Given a CSV column |name|, returns a pointer to the matching Label or nullptr
// if the column name is not recognised.
const Label* NameToLabel(std::string_view name) {
  // Recognised column names for origin URL, usernames and passwords.
  static constexpr auto kLabelMap =
      base::MakeFixedFlatMap<std::string_view, Label>({
          {"url", Label::kOrigin},
          {"website", Label::kOrigin},
          {"origin", Label::kOrigin},
          {"hostname", Label::kOrigin},
          {"login_uri", Label::kOrigin},

          {"username", Label::kUsername},
          {"user", Label::kUsername},
          {"login", Label::kUsername},
          {"account", Label::kUsername},
          {"login_username", Label::kUsername},

          {"password", Label::kPassword},
          {"login_password", Label::kPassword},
      });

  auto it = kLabelMap.find(name);
  return it != kLabelMap.end() ? &it->second : nullptr;
}

// Given |name| of a column, returns its note priority if it is a recognized
// note column name.
std::optional<size_t> GetNoteHeaderPriority(std::string_view name) {
  // Mapping names for note columns to their priorities.
  static constexpr auto kNoteLabelsPriority =
      base::MakeFixedFlatMap<std::string_view, size_t>({
          {"note", 0},
          {"notes", 1},
          {"comment", 2},
          {"comments", 3},
      });

  // TODO(crbug.com/40246323): record a metric if there multiple "note" columns
  // in one file and which names are used.

  auto it = kNoteLabelsPriority.find(name);
  return it != kNoteLabelsPriority.end() ? std::optional(it->second)
                                         : std::nullopt;
}

// Returns all the characters from the start of |input| until the first '\n',
// "\r\n" (exclusive) or the end of |input|. Cuts the returned part (inclusive
// of the line breaks) from |input|. Skips blocks of matching quotes. Examples:
// old input -> returned value, new input
// "ab\ncd" -> "ab", "cd"
// "\r\n" -> "", ""
// "abcd" -> "abcd", ""
// "\r" -> "\r", ""
// "a\"\n\"b" -> "a\"\n\"b", ""
std::string_view ConsumeCSVLine(std::string_view& input) {
  bool inside_quotes = false;
  bool last_char_was_cr = false;
  for (size_t current = 0; current < input.size(); ++current) {
    char c = input[current];
    switch (c) {
      case '\n':
        if (!inside_quotes) {
          const size_t eol_start = last_char_was_cr ? current - 1 : current;
          std::string_view ret = input.substr(0, eol_start);
          input.remove_prefix(current + 1);
          return ret;
        }
        break;
      case '"':
        inside_quotes = !inside_quotes;
        break;
      default:
        break;
    }
    last_char_was_cr = (c == '\r');
  }

  // The whole |input| is one line.
  return std::exchange(input, std::string_view());
}

}  // namespace

// static
base::expected<CSVPasswordSequence, CSVPassword::Status>
CSVPasswordSequence::Create(std::string_view csv) {
  if (csv.empty()) {
    return base::unexpected(CSVPassword::Status::kSyntaxError);
  }

  std::string_view header_row = ConsumeCSVLine(csv);

  std::optional<size_t> origin;
  std::optional<size_t> username;
  std::optional<size_t> password;
  std::optional<size_t> note;
  size_t note_column_priority = std::numeric_limits<size_t>::max();
  bool has_duplicate_label = false;

  CSVFieldParser parser(header_row);
  for (size_t col_index = 0; auto raw_name = parser.NextField(); ++col_index) {
    if (!raw_name->has_value()) {
      return base::unexpected(CSVPassword::Status::kSyntaxError);
    }

    std::string name = base::ToLowerASCII(
        base::TrimWhitespaceASCII(**raw_name, base::TRIM_ALL));
    if (const Label* label = NameToLabel(name)) {
      switch (*label) {
        case Label::kOrigin:
          if (std::exchange(origin, col_index).has_value()) {
            has_duplicate_label = true;
          }
          break;
        case Label::kUsername:
          if (std::exchange(username, col_index).has_value()) {
            has_duplicate_label = true;
          }
          break;
        case Label::kPassword:
          if (std::exchange(password, col_index).has_value()) {
            has_duplicate_label = true;
          }
          break;
      }
    } else if (std::optional<size_t> note_priority =
                   GetNoteHeaderPriority(name);
               note_priority.has_value() &&
               note_column_priority > *note_priority) {
      // If there are multiple columns matching one of the accepted "note"
      // field names, the one with the lowest priority should be used.
      note = col_index;
      note_column_priority = *note_priority;
    }
  }

  // Check that each of the required labels is uniquely assigned to a column.
  if (has_duplicate_label || !origin.has_value() || !username.has_value() ||
      !password.has_value()) {
    return base::unexpected(CSVPassword::Status::kSemanticError);
  }

  return CSVPasswordSequence(
      ColumnMap{
          .origin = *origin,
          .username = *username,
          .password = *password,
          .note = note,
      },
      csv);
}

CSVPasswordSequence::CSVPasswordSequence(ColumnMap column_map,
                                         std::string_view remaining_rows)
    : column_map_(column_map), remaining_rows_(remaining_rows) {}

CSVPasswordSequence::CSVPasswordSequence(CSVPasswordSequence&&) = default;
CSVPasswordSequence& CSVPasswordSequence::operator=(CSVPasswordSequence&&) =
    default;

CSVPasswordSequence::~CSVPasswordSequence() = default;

std::optional<CSVPassword> CSVPasswordSequence::NextPassword() {
  while (!remaining_rows_.empty()) {
    std::string_view row = base::TrimString(ConsumeCSVLine(remaining_rows_),
                                            "\r \t", base::TRIM_LEADING);
    if (row.empty()) {
      continue;
    }

    base::expected<GURL, std::string> url = base::unexpected("");
    std::string username;
    std::string password;
    std::string note;
    CSVPassword::Status status = CSVPassword::Status::kOK;

    CSVFieldParser parser(row);
    for (size_t field_idx = 0; auto field = parser.NextField(); ++field_idx) {
      if (!field->has_value()) {
        status = CSVPassword::Status::kSyntaxError;
        break;
      }
      if (field_idx == column_map_.origin) {
        GURL gurl(**field);
        if (gurl.is_valid()) {
          url = std::move(gurl);
        } else {
          url = base::unexpected(UnescapeCSVField(**field));
        }
      } else if (field_idx == column_map_.username) {
        username = UnescapeCSVField(**field);
      } else if (field_idx == column_map_.password) {
        password = UnescapeCSVField(**field);
      } else if (field_idx == column_map_.note) {
        note = UnescapeCSVField(**field);
      }
    }

    return CSVPassword(std::move(url), std::move(username), std::move(password),
                       std::move(note), status);
  }
  return std::nullopt;
}

}  // namespace password_manager
