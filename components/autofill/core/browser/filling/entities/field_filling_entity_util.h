// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ENTITIES_FIELD_FILLING_ENTITY_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ENTITIES_FIELD_FILLING_ENTITY_UTIL_H_

#include "base/containers/flat_set.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class EntityDataManager;
class FormStructure;

// Returns all fields in a `FormStructure` that are fillable by Autofill AI,
// taking into account the field type predictions and the available entities in
// `EntityDataManager`.
base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const EntityDataManager& edm);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ENTITIES_FIELD_FILLING_ENTITY_UTIL_H_
