// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_field_parser.h"

#include <string_view>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace password_manager {

CSVFieldParser::CSVFieldParser(std::string_view row) : row_(row) {}

CSVFieldParser::~CSVFieldParser() = default;

bool CSVFieldParser::NextField(std::string_view* field_contents) {
  DCHECK(HasMoreFields());
  if (fields_returned_ >= kMaxFields) {
    return false;
  }

  if (state_ != State::kInit) {
    state_ = State::kError;
    return false;
  }

  const size_t start = position_;
  do {
    UpdateState();
  } while (state_ != State::kInit && state_ != State::kError);

  if (state_ != State::kError) {
    DCHECK_GT(position_, start);  // There must have been at least the ','.
    *field_contents = row_.substr(start, position_ - start - 1);

    if (base::StartsWith(*field_contents, "\"")) {
      DCHECK(base::EndsWith(*field_contents, "\"")) << *field_contents;
      DCHECK_GE(field_contents->size(), 2u);
      field_contents->remove_prefix(1);
      field_contents->remove_suffix(1);
    }
    ++fields_returned_;
    return true;
  }
  return false;
}

char CSVFieldParser::ConsumeChar() {
  DCHECK_LE(position_, row_.size());
  // The default character to return once all from |row_| are consumed and
  // |position_| == |row_.size()|.
  char ret = ',';
  if (position_ < row_.size()) {
    ret = row_[position_];
  }
  ++position_;
  return ret;
}

void CSVFieldParser::UpdateState() {
  if (position_ > row_.size()) {
    // If in state |kInit| then the program attempts to read one field too many.
    DCHECK_NE(state_, State::kInit);
    // Otherwise a quotation mark was not matched before the end of input.
    state_ = State::kError;
    return;
  }

  char read = ConsumeChar();
  switch (state_) {
    case State::kInit:
      switch (read) {
        case ',':
          break;
        case '"':
          state_ = State::kQuoted;
          break;
        default:
          state_ = State::kPlain;
          break;
      }
      break;
    case State::kPlain:
      switch (read) {
        case ',':
          state_ = State::kInit;
          break;
        default:
          break;
      }
      break;
    case State::kQuoted:
      switch (read) {
        case '"':
          state_ = State::kAfter;
          break;
        default:
          break;
      }
      break;
    case State::kAfter:
      switch (read) {
        case ',':
          state_ = State::kInit;
          break;
        case '"':
          state_ = State::kQuoted;
          break;
        default:
          state_ = State::kError;
          break;
      }
      break;
    case State::kError:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace password_manager
