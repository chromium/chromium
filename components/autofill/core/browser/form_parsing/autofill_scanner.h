// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"

namespace autofill {

class FormFieldData;

// A helper class for parsing a stream of |FormFieldData|'s with lookahead.
class AutofillScanner {
 public:
  explicit AutofillScanner(
      const std::vector<raw_ptr<const FormFieldData>>& fields LIFETIME_BOUND);

  AutofillScanner(const AutofillScanner&) = delete;
  AutofillScanner& operator=(const AutofillScanner&) = delete;

  ~AutofillScanner();

  // Advances the cursor by one step, if possible.
  void Advance();

  // Returns the current field in the stream.
  const FormFieldData& Cursor() const;

  // Returns the field before Cursor(), or nullptr if there is none.
  const FormFieldData* Predecessor() const;

  // Returns |true| if the cursor has reached the end of the stream.
  bool IsEnd() const;

  // Restores the most recently saved cursor. See also |SaveCursor()|.
  void Rewind();

  // Repositions the cursor to the specified |index|. See also |SaveCursor()|.
  void RewindTo(size_t index);

  // Saves and returns the current cursor position. See also |Rewind()| and
  // |RewindTo()|.
  size_t SaveCursor();

  // Returns the current cursor position.
  size_t CursorPosition();

 private:
  // Indicates the current position in the stream, represented as a vector.
  std::vector<raw_ptr<const FormFieldData>>::const_iterator cursor_;

  // The most recently saved cursor.
  std::vector<raw_ptr<const FormFieldData>>::const_iterator saved_cursor_;

  // The beginning pointer for the stream.
  std::vector<raw_ptr<const FormFieldData>>::const_iterator begin_;

  // The past-the-end pointer for the stream.
  std::vector<raw_ptr<const FormFieldData>>::const_iterator end_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_
