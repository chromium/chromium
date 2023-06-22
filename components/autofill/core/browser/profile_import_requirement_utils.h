// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_IMPORT_REQUIREMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_IMPORT_REQUIREMENT_UTILS_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

// Returns true if minimum requirements for import of a given `profile` have
// been met. An address submitted via a form must have at least the fields
// required as determined by its country code. No verification of validity of
// the contents is performed. This is an existence check only. Assumes `profile`
// has been finalized.
bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& predicted_country_code,
                      const std::string& app_locale,
                      LogBuffer* import_log_buffer,
                      bool collect_metrics);

}  //  namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_IMPORT_REQUIREMENT_UTILS_H_
