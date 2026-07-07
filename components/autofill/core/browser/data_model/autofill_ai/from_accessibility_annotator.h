// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_FROM_ACCESSIBILITY_ANNOTATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_FROM_ACCESSIBILITY_ANNOTATOR_H_

#include <string>

#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

// Translates Autofill attribute names to entry types.
accessibility_annotator::MemoryDataType AttributeTypeToMemoryDataType(
    AttributeType type);

// Returns the localized name of the entry type.
std::u16string GetMemoryDataTypeNameForI18n(
    accessibility_annotator::MemoryDataType type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_FROM_ACCESSIBILITY_ANNOTATOR_H_
