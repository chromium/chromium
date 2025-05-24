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
class FormData;

// The same proto is used to make model requests and to collect data through the
// extension API. In the former case, only a subset of fields are necessary and
// thus the proto is only partially populated in this case.
enum class FormDataProtoConversionReason {
  kModelRequest = 0,
  kExtensionAPI = 1,
};

// Converts `form_data` to its corresponding form data proto, populating all
// fields necessary for the `conversion_reason`.
optimization_guide::proto::FormData ToFormDataProto(
    const FormData& form_data,
    FormDataProtoConversionReason conversion_reason);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
