// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_

namespace optimization_guide::proto {
class FormData;
}  // namespace optimization_guide::proto

namespace autofill {
class FormStructure;

// Converts `form_structure` to its corresponding form data proto.
optimization_guide::proto::FormData ToFormDataProto(
    const FormStructure& form_structure);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
