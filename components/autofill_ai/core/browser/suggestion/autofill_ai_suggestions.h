// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_SUGGESTIONS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_SUGGESTIONS_H_

#include "base/containers/span.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class EntityInstance;
class FormStructure;
struct Suggestion;

}  // namespace autofill

namespace autofill_ai {

// Creates filling suggestions using `autofill::EntityInstance`s.
std::vector<autofill::Suggestion> CreateFillingSuggestions(
    const autofill::FormStructure& form,
    autofill::FieldGlobalId field_global_id,
    base::span<const autofill::EntityInstance> entities,
    const std::string& app_locale);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_SUGGESTIONS_H_
