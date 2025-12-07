// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATAMODEL_AUTOFILL_AI_ENTITY_INSTANCE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATAMODEL_AUTOFILL_AI_ENTITY_INSTANCE_TEST_API_H_

#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

class EntityInstanceTestApi {
 public:
  explicit EntityInstanceTestApi(EntityInstance* entity)
      : entity_(CHECK_DEREF(entity)) {}
  ~EntityInstanceTestApi() = default;

  const std::string& frecency_override() const {
    return entity_->frecency_override_;
  }

 private:
  raw_ref<EntityInstance> entity_;
};

inline EntityInstanceTestApi test_api(EntityInstance& entity) {
  return EntityInstanceTestApi(&entity);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATAMODEL_AUTOFILL_AI_ENTITY_INSTANCE_TEST_API_H_
