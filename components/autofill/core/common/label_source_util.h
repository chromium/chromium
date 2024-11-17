// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_LABEL_SOURCE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_LABEL_SOURCE_UTIL_H_

#include <string>

#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// These values are persisted to logs. Do not rename the returned values.
std::string LabelSourceToString(FormFieldData::LabelSource label_source);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LABEL_SOURCE_UTIL_H_
