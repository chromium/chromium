// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_

#include <set>
#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {
class FormStructure;
class EntityInstance;
}  // namespace autofill

namespace autofill_ai {

// Separator to use between a certain entity label attributes, for example:
// "Passport · Jon Doe · Germany".
inline constexpr char16_t kLabelSeparator[] = u" · ";

// The maximum number of entity values/labels that can be used when
// disambiguating suggestions/entities. Used by suggestion generation and the
// settings page.
inline constexpr size_t kMaxNumberOfLabels = 2;

// Alias defining a list of labels available for each AutofillAi entity.
using EntitiesLabels =
    base::StrongAlias<class EntitiesLabelsTag,
                      std::vector<std::vector<std::u16string>>>;

// Returns whether the forms is eligible for the filling journey.
bool IsFormEligibleForFilling(const autofill::FormStructure& form);

// Given `entity_instances` returns `EntitiesLabels`, which will be a
// list of labels that can be used by an UI surface to display entities
// information. This is for example used by filling suggestions and the settings
// page.
// If `allow_only_disambiguating_types` is true, it will for example in the
// passport case return only values for name and country attributes, as they are
// part of the disambiguating attributes from the passport entity. If
// `return_at_least_one_label` is true, it makes sure that for each
// `entity_instances`, at least one label is present, even if it repeats across
// all other entities.
EntitiesLabels GetLabelsForEntities(
    base::span<const autofill::EntityInstance*> entity_instances,
    bool allow_only_disambiguating_types,
    bool return_at_least_one_label,
    const std::string& app_locale);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_
