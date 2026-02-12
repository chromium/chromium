// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_TYPES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_TYPES_H_

#include "base/containers/enum_set.h"

namespace accessibility_annotator {

enum class EntityType {
  kUnknown = 0,
  kFlight,
  kOrder,
  kShipment,
  kDriverLicense,
  kPassport,
  kNationalId,
  kMaxValue = kNationalId,
};

using EntityTypeEnumSet =
    base::EnumSet<EntityType, EntityType::kUnknown, EntityType::kMaxValue>;

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_TYPES_H_
