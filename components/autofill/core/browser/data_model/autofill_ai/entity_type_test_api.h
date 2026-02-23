// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_TYPE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_TYPE_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

class AttributeTypeTestApi {
 public:
  explicit AttributeTypeTestApi(AttributeType* attribute)
      : attribute_(CHECK_DEREF(attribute)) {}
  ~AttributeTypeTestApi() = default;

  FieldTypeSet storable_field_types() const {
    return attribute_->storable_field_types({});
  }

 private:
  raw_ref<AttributeType> attribute_;
};

inline AttributeTypeTestApi test_api(AttributeType& attribute) {
  return AttributeTypeTestApi(&attribute);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_ENTITY_TYPE_TEST_API_H_
