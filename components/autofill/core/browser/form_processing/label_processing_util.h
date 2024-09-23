// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_

#include <optional>
#include <string_view>
#include <vector>

namespace autofill {

// If parseable labels can be derived from |labels|, a vector of
// |std::u16string| is return that is aligned with |labels|.
// Parseable labels can be derived by splitting one label between multiple
// adjacent fields. If there aren't any changes to the labels, |std::nullopt|
// is returned.
std::optional<std::vector<std::u16string>> GetParseableLabels(
    const std::vector<std::u16string_view>& labels);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_LABEL_PROCESSING_UTIL_H_
