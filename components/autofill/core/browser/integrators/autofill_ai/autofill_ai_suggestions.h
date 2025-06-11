// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTIONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTIONS_H_

#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class EntityInstance;
class FormFieldData;
class FormStructure;
struct Suggestion;

// Creates filling suggestions using `autofill::EntityInstance`s.
std::vector<autofill::Suggestion> CreateFillingSuggestions(
    const FormStructure& form,
    const FormFieldData& trigger_field,
    base::span<const EntityInstance> entities,
    const std::string& app_locale);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTIONS_H_
