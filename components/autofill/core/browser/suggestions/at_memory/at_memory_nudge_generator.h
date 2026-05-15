// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AT_MEMORY_NUDGE_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AT_MEMORY_NUDGE_GENERATOR_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Generates suggestions for the AutoSuggest inactivity nudge. This nudge is
// shown when the user has been inactive for a while on a form field, suggesting
// to search in Chrome Memory.
class AtMemoryNudgeGenerator : public SuggestionGenerator {
 public:
  AtMemoryNudgeGenerator();
  AtMemoryNudgeGenerator(const AtMemoryNudgeGenerator&) = delete;
  AtMemoryNudgeGenerator& operator=(const AtMemoryNudgeGenerator&) = delete;
  ~AtMemoryNudgeGenerator() override;

  // SuggestionGenerator:
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      AutofillClient& client,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AT_MEMORY_NUDGE_GENERATOR_H_
