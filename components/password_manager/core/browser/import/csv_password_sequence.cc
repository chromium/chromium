// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_sequence.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_iterator.h"

namespace password_manager {

namespace {

// Given a CSV column |name|, returns a pointer to the matching
// CSVPassword::Label or nullptr if the column name is not recognised.
const CSVPassword::Label* NameToLabel(std::string_view name) {
  using Label = CSVPassword::Label;
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

          {"note", Label::kNote},
          {"notes", Label::kNote},
          {"comment", Label::kNote},
          {"comments", Label::kNote},
      });

  auto it = kLabelMap.find(name);
  return it != kLabelMap.end() ? &it->second : nullptr;
}

// Given |name| of a note column, returns its priority.
size_t GetNoteHeaderPriority(std::string_view name) {
  DCHECK_EQ(*NameToLabel(name), CSVPassword::Label::kNote);
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
  CHECK(it != kNoteLabelsPriority.end());
  return it->second;
}

}  // namespace

CSVPasswordSequence::CSVPasswordSequence(std::string csv)
    : csv_(std::move(csv)) {
  // Sanity check.
  if (csv_.empty()) {
    result_ = CSVPassword::Status::kSyntaxError;
    return;
  }
  data_rows_ = csv_;

  // Construct ColumnMap.
  std::string_view first = ConsumeCSVLine(&data_rows_);

  constexpr size_t kMaxPriority = 101;
  // Mapping "note column index" -> "header name priority".
  size_t note_column_index, note_column_priority = kMaxPriority;

  CSVFieldParser parser(first);
  for (size_t col_index = 0; auto raw_name = parser.NextField(); ++col_index) {
    if (!raw_name->has_value()) {
      result_ = CSVPassword::Status::kSyntaxError;
      return;
    }

    std::string name = base::ToLowerASCII(
        base::TrimWhitespaceASCII(**raw_name, base::TRIM_ALL));
    if (const CSVPassword::Label* label = NameToLabel(name)) {
      // If there are multiple columns matching one of the accepted "note" field
      // names, the one with the lowest priority should be used.
      if (*label == CSVPassword::Label::kNote) {
        size_t note_priority = GetNoteHeaderPriority(name);
        if (note_column_priority > note_priority) {
          note_column_index = col_index;
          note_column_priority = note_priority;
        }
        continue;
      }
      map_[col_index] = *label;
    }
  }

  if (note_column_priority != kMaxPriority) {
    map_[note_column_index] = CSVPassword::Label::kNote;
  }

  base::EnumSet<CSVPassword::Label> all_labels;
  for (const auto& [_, label] : map_) {
    if (all_labels.Has(label)) {
      // Multiple columns share the same label.
      result_ = CSVPassword::Status::kSemanticError;
      return;
    }
    all_labels.Put(label);
  }

  // Check that each of the required labels is assigned to a column.
  if (!all_labels.HasAll({CSVPassword::Label::kOrigin,
                          CSVPassword::Label::kUsername,
                          CSVPassword::Label::kPassword})) {
    result_ = CSVPassword::Status::kSemanticError;
    return;
  }
}

CSVPasswordSequence::CSVPasswordSequence(CSVPasswordSequence&&) = default;
CSVPasswordSequence& CSVPasswordSequence::operator=(CSVPasswordSequence&&) =
    default;

CSVPasswordSequence::~CSVPasswordSequence() = default;

CSVPasswordIterator CSVPasswordSequence::begin() const {
  if (result_ != CSVPassword::Status::kOK) {
    return end();
  }
  return CSVPasswordIterator(map_, data_rows_);
}

CSVPasswordIterator CSVPasswordSequence::end() const {
  return CSVPasswordIterator(map_, std::string_view());
}

}  // namespace password_manager
