// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;
class FormFieldData;

// Returns a map containing each field's parseable name **if** that name
// differs from FormFieldData::name().
//
// Parseable names are obtained by removing prefixes of FormFieldData::name().
//
// For example, in
//   <input name=id-12345-street-name>
//   <input name=id-12345-house-number>
//   <input name=id-12345-zip-code>
// the renderer assigns the full name to each <input>.
//
// GetParseableNames() strips the common prefixes in these names ("id-12345-").
// Prefixes are only removed if they are shared by all and a sufficient number
// of fields and they are sufficiently long.
base::flat_map<FieldGlobalId, std::u16string> GetParseableNames(
    base::span<const FormFieldData> fields);

base::flat_map<FieldGlobalId, std::u16string> GetParseableNames(
    base::span<const std::unique_ptr<AutofillField>> fields);

size_t FindLongestCommonAffixLengthForTest(  // IN-TEST
    base::span<std::u16string_view> strings,
    bool prefix);

void ComputeParseableNamesForTest(  // IN-TEST
    base::span<std::u16string_view> field_names);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
