// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// A helper class for parsing a stream of |FormFieldData|'s with lookahead.
class AutofillScanner {
 private:
  using Iterator = base::raw_span<const FormFieldData>::const_iterator;

 public:
  // The position of an AutofillScanner can be saved and restored.
  class Position {
   public:
    friend bool operator==(const Position&, const Position&) = default;

   private:
    friend class AutofillScanner;
    explicit Position(
        base::span<const FormFieldData>::const_iterator cursor LIFETIME_BOUND)
        : cursor_(cursor) {}
    Iterator cursor_;
  };

  // The scanner considers only `fields` for which `is_relevant()` is true.
  explicit AutofillScanner(base::span<const FormFieldData> fields
                               LIFETIME_BOUND,
                           bool (*is_relevant)(const FormFieldData&));

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

  [[nodiscard]] Position GetPosition() const LIFETIME_BOUND;
  void Restore(Position position);

  // This returns the distance since the beginning.
  //
  // Beware: This function takes linear time. Use GetPosition() if possible.
  size_t GetOffset() const;

 private:
  Iterator SkipBackward(Iterator iter) const;
  Iterator SkipForward(Iterator iter) const;

  bool (*const is_relevant_)(const FormFieldData&);

  base::raw_span<const FormFieldData> fields_;

  // Indicates the current position in the stream.
  Iterator cursor_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_SCANNER_H_
