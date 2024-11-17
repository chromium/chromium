// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_SERVICE_TEST_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_SERVICE_TEST_UTILS_H_

#include <memory>

#include "base/version_info/channel.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

class PrefService;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class DataTypeStoreService;
class DeviceInfoTracker;
}  // namespace syncer

namespace tab_groups::test {

// Factory method to create TabGroupSyncService that takes a model for testing.
std::unique_ptr<TabGroupSyncService> CreateTabGroupSyncService(
    std::unique_ptr<SavedTabGroupModel> model,
    syncer::DataTypeStoreService* data_type_store_service,
    PrefService* pref_service,
    syncer::DeviceInfoTracker* device_info_tracker,
    optimization_guide::OptimizationGuideDecider* optimization_guide,
    signin::IdentityManager* identity_manager);

}  // namespace tab_groups::test

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_SERVICE_TEST_UTILS_H_
