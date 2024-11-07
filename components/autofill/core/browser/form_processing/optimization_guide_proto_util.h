// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_

#include "base/containers/flat_map.h"
#include "components/autofill/core/common/unique_ids.h"

namespace optimization_guide::proto {
class FormData;
}  // namespace optimization_guide::proto

namespace autofill {
class FormStructure;
class FormData;

// Converts `form_structure` to its corresponding form data proto.
optimization_guide::proto::FormData ToFormDataProto(
    const FormData& form_data,
    const base::flat_map<FieldGlobalId, bool>& field_eligibility_map,
    const base::flat_map<FieldGlobalId, bool>& field_value_sensitivity_map);

// Convenience overload that calls the function above with an empty
// field_eligibility_map.
optimization_guide::proto::FormData ToFormDataProto(
    const FormStructure& form_structure);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
