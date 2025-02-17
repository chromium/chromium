// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/entities/field_filling_entity_util.h"

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const EntityDataManager& edm) {
  auto fillable_by_autofill_ai =
      [&, fillable_types =
              std::optional<FieldTypeSet>()](FieldType field_type) mutable {
        if (!fillable_types) {
          fillable_types.emplace();
          for (const EntityInstance& entity : edm.GetEntityInstances()) {
            for (const AttributeInstance& attribute : entity.attributes()) {
              fillable_types->insert(attribute.type().field_type());
            }
          }
        }
        return fillable_types->contains(field_type);
      };

  std::vector<FieldGlobalId> fields;
  for (const auto& field : form.fields()) {
    std::optional<FieldType> field_type =
        field->GetAutofillAiServerTypePredictions();
    if (field_type && fillable_by_autofill_ai(*field_type)) {
      fields.push_back(field->global_id());
    }
  }
  return std::move(fields);
}

}  // namespace autofill
