// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/sync_data_type_configuration.h"

#include "components/sync/model/model_type_change_processor.h"

namespace tab_groups {

SyncDataTypeConfiguration::SyncDataTypeConfiguration(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory model_type_store_factory)
    : change_processor(std::move(change_processor)),
      model_type_store_factory(std::move(model_type_store_factory)) {}

SyncDataTypeConfiguration::~SyncDataTypeConfiguration() = default;

}  // namespace tab_groups
