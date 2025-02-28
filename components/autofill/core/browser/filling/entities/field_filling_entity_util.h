// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ENTITIES_FIELD_FILLING_ENTITY_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ENTITIES_FIELD_FILLING_ENTITY_UTIL_H_

#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;
class EntityDataManager;
class EntityInstance;
class FormStructure;

// Returns all fields in a `FormStructure` that are fillable by Autofill AI,
// taking into account the field type predictions and the available entities in
// `EntityDataManager`.
base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const EntityDataManager& edm);

std::pair<std::u16string, std::optional<FieldType>>
GetFillValueAndTypeForEntity(const EntityInstance& entity,
                             const AutofillField& field,
                             mojom::ActionPersistence action_persistence,
                             const std::string& app_locale);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ENTITIES_FIELD_FILLING_ENTITY_UTIL_H_
