// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SYNC_DATA_TYPE_CONFIGURATION_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SYNC_DATA_TYPE_CONFIGURATION_H_

#include <memory>

#include "components/sync/model/model_type_store.h"

namespace syncer {
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace tab_groups {

// Configuration for a specific sync data type.
struct SyncDataTypeConfiguration {
  SyncDataTypeConfiguration(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory model_type_store_factory);
  ~SyncDataTypeConfiguration();

  std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor;
  syncer::OnceModelTypeStoreFactory model_type_store_factory;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SYNC_DATA_TYPE_CONFIGURATION_H_
