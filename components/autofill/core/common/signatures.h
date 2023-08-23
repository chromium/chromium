// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_SIGNATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_SIGNATURES_H_

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_piece.h"
#include "base/types/id_type.h"

namespace autofill {

struct FormData;
struct FormFieldData;

namespace internal {
using FormSignatureType = ::base::IdTypeU64<class FormSignatureMarker>;
using FieldSignatureType = ::base::IdTypeU32<class FieldSignatureMarker>;
}  // namespace internal

// The below strong aliases are defined as subclasses instead of typedefs in
// order to avoid having to define out-of-line constructors in all structs that
// contain signatures.

class FormSignature : public internal::FormSignatureType {
  using internal::FormSignatureType::IdType;
};

class FieldSignature : public internal::FieldSignatureType {
  using internal::FieldSignatureType::IdType;
};

// Calculates form signature based on |form_data|.
FormSignature CalculateFormSignature(const FormData& form_data);

// Returns a more generic form signature than CalculateFormSignature. It is used
// in cases where the web form has an unstable form signature (a random
// signature due to changing form or field names at each page load).
FormSignature CalculateAlternativeFormSignature(const FormData& form_data);

// Calculates field signature based on |field_name| and |field_type|.
FieldSignature CalculateFieldSignatureByNameAndType(
    base::StringPiece16 field_name,
    base::StringPiece field_type);

// Calculates field signature based on |field_data|. This function is a proxy to
// |CalculateFieldSignatureByNameAndType|.
FieldSignature CalculateFieldSignatureForField(const FormFieldData& field_data);

// Returns 64-bit hash of the string.
uint64_t StrToHash64Bit(base::StringPiece str);

// Returns 32-bit hash of the string.
uint32_t StrToHash32Bit(base::StringPiece str);

// Reduce FieldSignature space (in UKM) to a small range for privacy reasons.
int64_t HashFormSignature(FormSignature form_signature);

// Reduce FieldSignature space (in UKM) to a small range for privacy reasons.
int64_t HashFieldSignature(FieldSignature field_signature);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_SIGNATURES_H_
