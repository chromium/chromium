// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SUGGESTION_H_

#include <map>
#include <string>

#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;
class FormStructure;

using OtpFillData = std::map<FieldGlobalId, std::u16string>;

// When an OTP suggestion is selected, this method is called to get the fill
// data, which contains information about what value to fill into what field.
OtpFillData CreateFillDataForOtpSuggestion(const FormStructure& form,
                                           const AutofillField& trigger_field,
                                           const std::u16string& otp_value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SUGGESTION_H_
