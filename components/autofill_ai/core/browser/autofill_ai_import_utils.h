// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_IMPORT_UTILS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_IMPORT_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
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

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_IMPORT_UTILS_H_
