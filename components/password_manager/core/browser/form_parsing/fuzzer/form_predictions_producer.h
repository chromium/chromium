// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_PREDICTIONS_PRODUCER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_PREDICTIONS_PRODUCER_H_

#include "components/autofill/core/common/form_data.h"

class FuzzedDataProvider;

namespace password_manager {

struct FormPredictions;

// Generates a |FormPredictions| for the given form data, based on values
// obtained via |provider|. See https://goo.gl/29t6VH for a detailed design.
FormPredictions GenerateFormPredictions(const autofill::FormData& form_data,
                                        FuzzedDataProvider& provider);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_PREDICTIONS_PRODUCER_H_
