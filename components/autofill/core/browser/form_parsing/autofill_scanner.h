// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace autofill {

class AutofillField;

// A helper class for parsing a stream of |AutofillField|'s with lookahead.
class AutofillScanner {
 public:
  explicit AutofillScanner(
      const std::vector<raw_ptr<AutofillField, VectorExperimental>>& fields);
  explicit AutofillScanner(
      const std::vector<std::unique_ptr<AutofillField>>& fields);

  AutofillScanner(const AutofillScanner&) = delete;
  AutofillScanner& operator=(const AutofillScanner&) = delete;

  ~AutofillScanner();

  // Advances the cursor by one step, if possible.
  void Advance();

  // Returns the current field in the stream, or |NULL| if there are no more
  // fields in the stream.
  AutofillField* Cursor() const;

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
  void Init(
      const std::vector<raw_ptr<AutofillField, VectorExperimental>>& fields);

  // Indicates the current position in the stream, represented as a vector.
  std::vector<raw_ptr<AutofillField, VectorExperimental>>::const_iterator
      cursor_;

  // The most recently saved cursor.
  std::vector<raw_ptr<AutofillField, VectorExperimental>>::const_iterator
      saved_cursor_;

  // The beginning pointer for the stream.
  std::vector<raw_ptr<AutofillField, VectorExperimental>>::const_iterator
      begin_;

  // The past-the-end pointer for the stream.
  std::vector<raw_ptr<AutofillField, VectorExperimental>>::const_iterator end_;

  // The storage of non-owning pointers, used for the unique_ptr constructor.
  std::vector<raw_ptr<AutofillField, VectorExperimental>> non_owning_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_
