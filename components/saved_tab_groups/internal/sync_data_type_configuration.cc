// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"

#include "components/sync/model/data_type_local_change_processor.h"

namespace tab_groups {

SyncDataTypeConfiguration::SyncDataTypeConfiguration(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory data_type_store_factory)
    : change_processor(std::move(change_processor)),
      data_type_store_factory(std::move(data_type_store_factory)) {}

SyncDataTypeConfiguration::~SyncDataTypeConfiguration() = default;

}  // namespace tab_groups
