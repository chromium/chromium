// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_FROM_ACCESSIBILITY_ANNOTATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_FROM_ACCESSIBILITY_ANNOTATOR_H_

#include <optional>

#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

extern constinit const DenseSet<accessibility_annotator::EntityType>
    kAllEntityTypesSharedWithAccessibilityAnnotator;

// Maps Accessibility Annotator entity types to Autofill AI entity types.
//
// An Accessibility Annotator entity type may have no corresponding Autofill AI
// equivalent. In that case, the `DenseSet<EntityType>` has no such entry.
DenseSet<EntityType> FromAccessibilityAnnotator(
    accessibility_annotator::EntityTypeEnumSet src_entities);

// Maps an Accessibility Annotator entity instance to an Autofill AI entity
// instance.
//
// An Accessibility Annotator entity type may have no corresponding Autofill AI
// equivalent. In that case, the `DenseSet<EntityType>` has no such entry.
std::optional<EntityInstance> FromAccessibilityAnnotator(
    const accessibility_annotator::Entity& entity);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_FROM_ACCESSIBILITY_ANNOTATOR_H_
