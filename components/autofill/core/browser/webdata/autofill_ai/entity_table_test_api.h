// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_TABLE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_TABLE_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "sql/database.h"

namespace autofill {

class EntityTableTestApi {
 public:
  explicit EntityTableTestApi(EntityTable* entity_table)
      : entity_table_(*entity_table) {}

  sql::Database* db() { return entity_table_->db(); }

  std::vector<EntityInstance::EntityMetadata> GetMetadataEntries() const {
    std::vector<EntityInstance::EntityMetadata> all_metadata;
    for (const auto& [guid, metadata] : entity_table_->LoadMetadata()) {
      all_metadata.push_back(metadata);
    }
    return all_metadata;
  }

 private:
  const raw_ref<EntityTable> entity_table_;
};

inline EntityTableTestApi test_api(EntityTable& entity_table) {
  return EntityTableTestApi(&entity_table);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_TABLE_TEST_API_H_
