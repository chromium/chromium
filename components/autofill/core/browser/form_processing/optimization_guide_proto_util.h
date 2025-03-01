// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_

#include "components/autofill/core/common/unique_ids.h"

namespace optimization_guide::proto {
class FormData;
}  // namespace optimization_guide::proto

namespace autofill {
class FormStructure;
class FormData;

// Converts `form_structure` to its corresponding form data proto. It does not
// populate `field_value` fields.
optimization_guide::proto::FormData ToFormDataProto(const FormData& form_data);

// Convenience overload that calls the function above.
// TODO(crbug.com/395038288): Remove once user annotations are removed.
optimization_guide::proto::FormData ToFormDataProto(
    const FormStructure& form_structure);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
