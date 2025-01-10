// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_type.h"

#include "base/types/cxx23_to_underlying.h"

namespace autofill {

FieldType AttributeTypeNameToFieldType(AttributeTypeName a) {
  switch (a) {
    case AttributeTypeName::kPassportName:
      return NAME_FULL;
    default:
      return NO_SERVER_DATA;
  }
  NOTREACHED();
}

EntityType AttributeType::entity_type() const {
  return EntityType(AttributeTypeNameToEntityTypeName(name_));
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << base::to_underlying(t.name());
}

std::ostream& operator<<(std::ostream& os, AttributeType t) {
  return os << base::to_underlying(t.name());
}

}  // namespace autofill
