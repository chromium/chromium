// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_IMPORT_UTILS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_IMPORT_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {
class AutofillField;
class EntityInstance;
}  // namespace autofill

namespace autofill_ai {

bool AttributesMeetImportConstraints(
    autofill::EntityType entity_type,
    autofill::DenseSet<autofill::AttributeType> attributes);

// Returns import candidates.
std::vector<autofill::EntityInstance> GetPossibleEntitiesFromSubmittedForm(
    base::span<const std::unique_ptr<autofill::AutofillField>> fields,
    const std::string& app_locale);

// Returns the localized date value of `attribute`, if its FieldType is a date.
// Otherwise returns std::nullopt.
//
// The localization depends on the currently set ICU locale
// (base::i18n::GetConfiguredLocale()).
//
// For example, if the attribute value is "2025-01-31" and the currently set ICU
// locale is "en_US", the returned string is "Jan 31, 2025".
std::optional<std::u16string> MaybeGetLocalizedDate(
    const autofill::AttributeInstance& attribute);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_IMPORT_UTILS_H_
