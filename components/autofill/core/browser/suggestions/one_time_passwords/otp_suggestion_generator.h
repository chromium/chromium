// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_

#include <vector>

#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

// Generates OTP suggestions from the provided vector of retrieved OTP values.
// TODO(crbug.com/409962888): Cleanup once AutofillNewSuggestionGeneration is
// launched.
std::vector<Suggestion> BuildOtpSuggestions(
    std::vector<std::string> one_time_passwords,
    const FieldGlobalId& field_id);

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_
