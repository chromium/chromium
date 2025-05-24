// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_

#include <string_view>
#include <vector>

namespace autofill {

// Given the `labels` of all fields in a form in order, this logic attempts
// to split labels among consecutive fields by common separators. In particular,
// for cases like
//  Street name and house number <input><input>
// label inference will assign the full string as the label of the first
// <input>. Depending on the exact DOM structure, the second <input> might
// receive the same label or no label at all.
// This logic finds intervals of this kind, splits the first label in the
// interval by common separators ("and") and overwrites all labels in the
// interval by the result of the split. In the example above, the labels become
// "Street name" and "house number", respectively.
// The function returns a vector of the same size as `labels`, containing the
// processed labels. Since the function operates on string_views, `label[i]` of
// the result might reference (part of) `label[i-1]` of the input.
std::vector<std::u16string_view> GetParseableLabels(
    std::vector<std::u16string_view> labels);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_
