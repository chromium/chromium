// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PROTO_UTIL_H_

namespace autofill {
class FormData;
}  // namespace autofill

namespace optimization_guide::proto {
class FormData;
class AXTreeUpdate;
}  // namespace optimization_guide::proto

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace optimization_guide {

// Converts `form_data` to its corresponding form data proto.
optimization_guide::proto::FormData ToFormDataProto(
    const autofill::FormData& form_data);

// Populate the AXTreeUpdate proto structure from the ui structure.
void PopulateAXTreeUpdateProto(
    const ui::AXTreeUpdate& source,
    optimization_guide::proto::AXTreeUpdate* destination);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
