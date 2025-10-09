// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_FIELD_FILLING_ENTITY_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_FIELD_FILLING_ENTITY_UTIL_H_

#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AddressNormalizer;
class AutofillClient;
class AutofillField;
struct AutofillFieldWithAttributeType;
class EntityInstance;
class FormStructure;

// Returns the entities from EntityDataManager::GetEntityInstances() for which
// filling is enabled.
std::vector<const EntityInstance*> GetFillableEntityInstances(
    const AutofillClient& client);

// Returns all fields in a `FormStructure` that are fillable by Autofill AI,
// taking into account whether AutofillAI filling is enabled as well as the
// field type predictions and the available entities in `EntityDataManager`.
base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const AutofillClient& client);

// Returns the value from `entity` to fill into `field`.
std::u16string GetFillValueForEntity(
    const EntityInstance& entity,
    base::span<const AutofillFieldWithAttributeType> fields_and_types,
    const AutofillField& field,
    mojom::ActionPersistence action_persistence,
    const std::string& app_locale,
    AddressNormalizer* address_normalizer);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_FIELD_FILLING_ENTITY_UTIL_H_
