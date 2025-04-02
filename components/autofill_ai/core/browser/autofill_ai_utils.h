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
class AttributeType;
class EntityInstance;
}  // namespace autofill

namespace autofill_ai {

// Separator to use between a certain entity label attributes, for example:
// "Passport · Jon Doe · Germany".
inline constexpr char16_t kLabelSeparator[] = u" · ";

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
// `attribute_types_to_exclude` is used to exclude specific attribute types from
// the list of available labels.
EntitiesLabels GetLabelsForEntities(
    base::span<const autofill::EntityInstance*> entity_instances,
    const autofill::DenseSet<autofill::AttributeType>&
        attribute_types_to_exclude,
    const std::string& app_locale);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_
