// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/at_memory/at_memory_nudge_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"

namespace autofill {

AtMemoryNudgeGenerator::AtMemoryNudgeGenerator() = default;

AtMemoryNudgeGenerator::~AtMemoryNudgeGenerator() = default;

void AtMemoryNudgeGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  std::vector<Suggestion> suggestions;
  // TODO(crbug.com/489659527): Localize the string.
  Suggestion suggestion(u"Try searching in Chrome Memory",
                        SuggestionType::kAtMemoryInactivityNudge);
  suggestions.push_back(std::move(suggestion));
  std::move(callback).Run(
      {SuggestionDataSource::kAtMemoryInactivityNudge, std::move(suggestions)});
}

}  // namespace autofill
