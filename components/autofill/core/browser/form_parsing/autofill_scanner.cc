// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

AutofillScanner::AutofillScanner(base::span<const FormFieldData> fields)
    : fields_(fields), cursor_(fields_.begin()) {}

AutofillScanner::~AutofillScanner() = default;

void AutofillScanner::Advance() {
  CHECK(!IsEnd());
  ++cursor_;
}

const FormFieldData& AutofillScanner::Cursor() const {
  CHECK(!IsEnd());
  return *cursor_;
}

const FormFieldData* AutofillScanner::Predecessor() const {
  return cursor_ != fields_.begin() ? &*std::prev(cursor_) : nullptr;
}

bool AutofillScanner::IsEnd() const {
  return cursor_ == fields_.end();
}

AutofillScanner::Position AutofillScanner::GetPosition() const {
  return Position(cursor_);
}

void AutofillScanner::Restore(Position position) {
  cursor_ = position.cursor_;
}

size_t AutofillScanner::GetOffset() const {
  return cursor_ - fields_.begin();
}

}  // namespace autofill
