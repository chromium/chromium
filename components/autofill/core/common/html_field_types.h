// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_

#include <stdint.h>
#include "base/strings/string_piece_forward.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

using HtmlFieldMode = ::autofill::mojom::HtmlFieldMode;
using HtmlFieldType = ::autofill::mojom::HtmlFieldType;

// Returns a StringPiece describing `type`. As the StringPiece points to a
// static string, you don't need to worry about memory deallocation.
//
// The returned string is intentionally kept in the old constant style
// ("HTML_TYPE_FOO" rather than "kFoo" or "HtmlFieldType::kFoo") because
// external tools may depend on it.
base::StringPiece FieldTypeToStringPiece(HtmlFieldType type);

// Maps HtmlFieldMode::kBilling and HtmlFieldMode::kShipping to
// their string constants, as specified in the autocomplete standard.
base::StringPiece HtmlFieldModeToStringPiece(HtmlFieldMode mode);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_
