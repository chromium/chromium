// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_FUZZED_PRODUCER_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_FUZZED_PRODUCER_H_

#include "components/autofill/core/common/form_data.h"

class FuzzedDataProvider;

namespace autofill {

// Generates a |FormData| object based on values obtained via |provider|. See
// https://goo.gl/29t6VH for a detailed design.
FormData GenerateFormData(FuzzedDataProvider& provider);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_FUZZED_PRODUCER_H_
