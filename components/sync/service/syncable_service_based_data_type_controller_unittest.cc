// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/syncable_service_based_data_type_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class SyncableServiceBasedDataTypeControllerTest : public testing::Test {
 public:
  SyncableServiceBasedDataTypeControllerTest()
      : store_(DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}
  ~SyncableServiceBasedDataTypeControllerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
  const std::unique_ptr<DataTypeStore> store_;
};

TEST_F(SyncableServiceBasedDataTypeControllerTest, HandlesNullService) {
  // Create a controller with a null SyncableService.
  SyncableServiceBasedDataTypeController controller(
      PREFERENCES, DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
      /*syncable_service=*/nullptr, base::DoNothing(),
      SyncableServiceBasedDataTypeController::DelegateMode::
          kTransportModeWithSingleModel);

  // Call various methods on the controller. These should essentially all do
  // nothing, but not crash.
  controller.GetPreconditionState();
  controller.LoadModels(ConfigureContext{.cache_guid = "cache_guid"},
                        base::DoNothing());
  controller.HasUnsyncedData(base::DoNothing());
  controller.GetTypeEntitiesCount(base::DoNothing());
  controller.Stop(SyncStopMetadataFate::CLEAR_METADATA, base::DoNothing());
}

}  // namespace
}  // namespace syncer
