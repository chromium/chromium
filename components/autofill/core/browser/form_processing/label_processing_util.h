// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class FormFieldData;
class AutofillField;

// Returns a map containing each field's parseable label **if** that label
// differs from FormFieldData::label().
//
// Parseable labels are obtained by splitting FormFieldData::label() by typical
// separators.
//
// For example, in
//   Street name and house number <input><input>
// the renderer's label inference assigns the full string to the first <input>.
// Depending on the exact DOM structure, the second <input> might receive the
// same label or no label at all.
//
// GetParseableLabels() finds intervals of this kind, splits the first label in
// the interval by typical separators (such as "and") and overwrites all labels
// in the interval by the result of the split. In the example above, the
// parseable labels are "Street name" and "house number", respectively.
base::flat_map<FieldGlobalId, std::u16string> GetParseableLabels(
    base::span<const FormFieldData> fields);

base::flat_map<FieldGlobalId, std::u16string> GetParseableLabels(
    base::span<const std::unique_ptr<AutofillField>> fields);

std::vector<std::u16string_view> GetParseableLabelsForTest(  // IN-TEST
    std::vector<std::u16string_view> labels);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_
