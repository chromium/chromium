// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

AutofillScanner::AutofillScanner(
    const std::vector<raw_ptr<AutofillField, VectorExperimental>>& fields) {
  Init(fields);
}

AutofillScanner::AutofillScanner(
    const std::vector<std::unique_ptr<AutofillField>>& fields) {
  for (const auto& field : fields)
    non_owning_.push_back(field.get());

  Init(non_owning_);
}

AutofillScanner::~AutofillScanner() = default;

void AutofillScanner::Advance() {
  DCHECK(!IsEnd());
  ++cursor_;
}

AutofillField* AutofillScanner::Cursor() const {
  if (IsEnd()) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  return *cursor_;
}

bool AutofillScanner::IsEnd() const {
  return cursor_ == end_;
}

void AutofillScanner::Rewind() {
  DCHECK(saved_cursor_ != end_);
  cursor_ = saved_cursor_;
  saved_cursor_ = end_;
}

void AutofillScanner::RewindTo(size_t index) {
  DCHECK(index < static_cast<size_t>(end_ - begin_));
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

void AutofillScanner::Init(
    const std::vector<raw_ptr<AutofillField, VectorExperimental>>& fields) {
  cursor_ = fields.begin();
  saved_cursor_ = fields.begin();
  begin_ = fields.begin();
  end_ = fields.end();
}

}  // namespace autofill
