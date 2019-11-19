// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_DATA_PRODUCER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_DATA_PRODUCER_H_

#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

namespace password_manager {

class DataAccessor;

// Generates a |FormData| and |predictions| object based on values obtained via
// |accessor|. See https://goo.gl/29t6VH for a detailed design.
autofill::FormData GenerateWithDataAccessor(DataAccessor* accessor,
                                            FormPredictions* predictions);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_DATA_PRODUCER_H_
