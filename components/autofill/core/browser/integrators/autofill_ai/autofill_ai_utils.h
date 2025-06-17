// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_UTILS_H_

#include <set>
#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

class FormStructure;
class EntityInstance;

// Separator to use between a certain entity label attributes, for example:
// "Passport · Jon Doe · Germany".
inline constexpr char16_t kLabelSeparator[] = u" · ";

// The maximum number of entity values/labels that can be used when
// disambiguating suggestions/entities. Used by suggestion generation and the
// settings page.
inline constexpr size_t kMaxNumberOfLabels = 2;

// During label computation, every entity's label is a vector of non-empty
// strings (which the UI later concatenates).
using EntityLabel = std::vector<std::u16string>;

// Returns whether the form is eligible for the filling journey.
bool IsFormEligibleForFilling(const FormStructure& form);

// Returns a vector of EntityLabels, with one entry for each EntityInstance in
// `entities`.
//
// That is, the `i`th element of the returned vector corresponds to
// `entities[i]`. The individual EntityLabels may be empty, but the strings they
// contain are non-empty.
//
// This is for example used by filling suggestions and the settings page.
//
// If `allow_only_disambiguating_types` is true, only disambiguating types are
// considered. For example, for a passport, the name and country are considered,
// but the number is not.
//
// If `return_at_least_one_label` is true and the attributes agree on all
// (potentially disambiguating) attribute values, then we fall back to some of
// those values they agree on.
std::vector<EntityLabel> GetLabelsForEntities(
    base::span<const EntityInstance*> entities,
    bool allow_only_disambiguating_types,
    bool return_at_least_one_label,
    const std::string& app_locale);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_UTILS_H_
