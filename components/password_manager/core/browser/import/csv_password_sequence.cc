// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_sequence.h"

#include <set>
#include <string>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_iterator.h"

namespace password_manager {

namespace {

// Given a CSV column |name|, returns a pointer to the matching
// CSVPassword::Label or nullptr if the column name is not recognised.
const CSVPassword::Label* NameToLabel(base::StringPiece name) {
  using Label = CSVPassword::Label;
  // Recognised column names for origin URL, usernames and passwords.
  static constexpr auto kLabelMap =
      base::MakeFixedFlatMap<base::StringPiece, Label>({
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

  std::string trimmed_name;
  // Trim leading/trailing whitespaces from |name|.
  base::TrimString(name, " ", &trimmed_name);
  auto* it = kLabelMap.find(base::ToLowerASCII(trimmed_name));
  return it != kLabelMap.end() ? &it->second : nullptr;
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
  base::StringPiece first = ConsumeCSVLine(&data_rows_);
  size_t col_index = 0;
  for (CSVFieldParser parser(first); parser.HasMoreFields(); ++col_index) {
    base::StringPiece name;
    if (!parser.NextField(&name)) {
      result_ = CSVPassword::Status::kSyntaxError;
      return;
    }

    if (const CSVPassword::Label* label = NameToLabel(name))
      map_[col_index] = *label;
  }

  // Check that each of the three labels is assigned to exactly one column.
  if (map_.size() != CSVPassword::kLabelCount) {
    result_ = CSVPassword::Status::kSemanticError;
    return;
  }
  base::flat_set<CSVPassword::Label> all_labels;
  for (const auto& kv : map_) {
    if (!all_labels.insert(kv.second).second) {
      // More columns share the same label.
      result_ = CSVPassword::Status::kSemanticError;
      return;
    }
  }
}

CSVPasswordSequence::CSVPasswordSequence(CSVPasswordSequence&&) = default;
CSVPasswordSequence& CSVPasswordSequence::operator=(CSVPasswordSequence&&) =
    default;

CSVPasswordSequence::~CSVPasswordSequence() = default;

CSVPasswordIterator CSVPasswordSequence::begin() const {
  if (result_ != CSVPassword::Status::kOK)
    return end();
  return CSVPasswordIterator(map_, data_rows_);
}

CSVPasswordIterator CSVPasswordSequence::end() const {
  return CSVPasswordIterator(map_, base::StringPiece());
}

}  // namespace password_manager
