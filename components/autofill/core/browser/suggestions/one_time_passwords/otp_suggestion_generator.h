// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace one_time_tokens {
class OneTimeToken;
}  // namespace one_time_tokens

namespace autofill {

class OtpManager;

// Generates OTP suggestions from the provided span of retrieved OTP tokens.
// TODO(crbug.com/409962888): Cleanup once AutofillNewSuggestionGeneration is
// launched.
std::vector<Suggestion> BuildOtpSuggestions(
    base::span<const one_time_tokens::OneTimeToken> one_time_tokens);

// Returns the OTPs from `one_time_tokens` that are worth suggesting on
// `trigger_field`, given the field's current value:
// - OTPs equal to the field's value are dropped because filling them would not
//   change the field.
// - Unless the field is autofilled (in which case a different OTP, e.g. a
//   resent one, may replace the filled value), OTPs are dropped if the field's
//   value is not a prefix of them, e.g. because the user is manually typing a
//   different code.
// Values are compared case-insensitively for ASCII letters and ignore
// whitespace and ASCII punctuation, such as separators or input mask
// placeholders.
// TODO(crbug.com/409962888): Make this an implementation detail of
// `OtpSuggestionGenerator` once AutofillNewSuggestionGeneration is launched.
std::vector<one_time_tokens::OneTimeToken> GetOtpsToSuggest(
    std::vector<one_time_tokens::OneTimeToken> one_time_tokens,
    const FormFieldData& trigger_field);

// A `SuggestionGenerator` for `FillingProduct::kOneTimePassword`.
class OtpSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit OtpSuggestionGenerator(OtpManager& otp_manager);
  ~OtpSuggestionGenerator() override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      AutofillClient& client,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

 private:
  void OnOtpReturned(
      base::OnceCallback<void(ReturnedSuggestions)> callback,
      const FormFieldData& trigger_field,
      std::vector<one_time_tokens::OneTimeToken> one_time_tokens);

  raw_ref<OtpManager> otp_manager_;

  base::WeakPtrFactory<OtpSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_OTP_SUGGESTION_GENERATOR_H_
