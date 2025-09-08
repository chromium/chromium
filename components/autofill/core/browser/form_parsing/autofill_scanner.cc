// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

AutofillScanner::AutofillScanner(
    const std::vector<raw_ptr<const FormFieldData>>& fields) {
  cursor_ = fields.begin();
  saved_cursor_ = fields.begin();
  begin_ = fields.begin();
  end_ = fields.end();
}

AutofillScanner::~AutofillScanner() = default;

void AutofillScanner::Advance() {
  CHECK(!IsEnd());
  ++cursor_;
}

const FormFieldData& AutofillScanner::Cursor() const {
  CHECK(!IsEnd());
  return **cursor_;
}

const FormFieldData* AutofillScanner::Predecessor() const {
  return cursor_ != begin_ ? *std::prev(cursor_) : nullptr;
}

bool AutofillScanner::IsEnd() const {
  return cursor_ == end_;
}

void AutofillScanner::Rewind() {
  CHECK(saved_cursor_ != end_);
  cursor_ = saved_cursor_;
  saved_cursor_ = end_;
}

void AutofillScanner::RewindTo(size_t index) {
  CHECK_LE(index, static_cast<size_t>(std::distance(begin_, end_)));
  cursor_ = begin_ + index;
  saved_cursor_ = end_;
}

size_t AutofillScanner::SaveCursor() {
  saved_cursor_ = cursor_;
  return static_cast<size_t>(cursor_ - begin_);
}

size_t AutofillScanner::CursorPosition() {
  return static_cast<size_t>(cursor_ - begin_);
}

}  // namespace autofill
