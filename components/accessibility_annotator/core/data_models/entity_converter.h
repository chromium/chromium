// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_CONVERTER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_CONVERTER_H_

#include <optional>

#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"

namespace accessibility_annotator {

// Converts a sync_pb::AccessibilityAnnotationSpecifics proto to an Entity.
// Returns std::nullopt if the conversion fails or if the specific type is not
// supported.
std::optional<Entity> CreateEntityFromSpecifics(
    const sync_pb::AccessibilityAnnotationSpecifics& specifics);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_CONVERTER_H_
