// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

AutofillScanner::AutofillScanner(base::span<const FormFieldData> fields,
                                 bool (*is_relevant)(const FormFieldData&))
    : is_relevant_(is_relevant),
      fields_(fields),
      cursor_(SkipForward(fields_.begin())) {}

AutofillScanner::~AutofillScanner() = default;

void AutofillScanner::Advance() {
  CHECK(!IsEnd());
  cursor_ = SkipForward(cursor_ + 1);
}

AutofillScanner::Iterator AutofillScanner::SkipBackward(Iterator it) const {
  while (it != fields_.begin() && !is_relevant_(*it)) {
    --it;
  }
  return is_relevant_(*it) ? it : fields_.end();
}

AutofillScanner::Iterator AutofillScanner::SkipForward(Iterator it) const {
  while (it != fields_.end() && !is_relevant_(*it)) {
    ++it;
  }
  return it;
}

const FormFieldData& AutofillScanner::Cursor() const {
  CHECK(!IsEnd());
  CHECK(is_relevant_(*cursor_));
  return *cursor_;
}

const FormFieldData* AutofillScanner::Predecessor() const {
  CHECK(!IsEnd());
  CHECK(is_relevant_(*cursor_));
  if (cursor_ == fields_.begin()) {
    return nullptr;
  }
  const Iterator it = SkipBackward(std::prev(cursor_));
  if (it == fields_.end()) {
    return nullptr;
  }
  return &*it;
}

bool AutofillScanner::IsEnd() const {
  return cursor_ == fields_.end();
}

AutofillScanner::Position AutofillScanner::GetPosition() const {
  return Position(cursor_);
}

void AutofillScanner::Restore(Position position) {
  cursor_ = SkipForward(position.cursor_);
}

size_t AutofillScanner::GetOffset() const {
  size_t position = 0;
  Iterator it = SkipForward(fields_.begin());
  while (it != cursor_) {
    it = SkipForward(it + 1);
    ++position;
  }
  return position;
}

}  // namespace autofill
