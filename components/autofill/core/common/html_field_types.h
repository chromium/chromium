// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_

#include <string_view>

#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

using HtmlFieldMode = ::autofill::mojom::HtmlFieldMode;
using HtmlFieldType = ::autofill::mojom::HtmlFieldType;

// Returns a string view describing `type`.
//
// The returned string is intentionally kept in the old constant style
// ("HTML_TYPE_FOO" rather than "kFoo" or "HtmlFieldType::kFoo") because
// external tools may depend on it.
std::string_view FieldTypeToStringView(HtmlFieldType type);

// Returns a string describing `type`.
//
// The returned string is intentionally kept in the old constant style
// ("HTML_TYPE_FOO" rather than "kFoo" or "HtmlFieldType::kFoo") because
// external tools may depend on it.
std::string FieldTypeToString(HtmlFieldType type);

// Maps HtmlFieldMode::kBilling and HtmlFieldMode::kShipping to
// their string view constants, as specified in the autocomplete standard.
std::string_view HtmlFieldModeToStringView(HtmlFieldMode mode);

// Maps HtmlFieldMode::kBilling and HtmlFieldMode::kShipping to
// their string constants, as specified in the autocomplete standard.
std::string HtmlFieldModeToString(HtmlFieldMode mode);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_HTML_FIELD_TYPES_H_
