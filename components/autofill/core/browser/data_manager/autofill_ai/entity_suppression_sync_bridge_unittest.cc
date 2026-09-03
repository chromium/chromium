// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::NiceMock;

sync_pb::AutofillEntitySuppressionSpecifics CreateTestSpecifics(
    const std::string& guid) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid(guid);
  return specifics;
}

syncer::EntityData EntityFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  syncer::EntityData entity;
  entity.name = specifics.guid();
  *entity.specifics.mutable_autofill_entity_suppression() = specifics;
  return entity;
}

class EntitySuppressionSyncBridgeTest : public testing::Test {
 public:
  EntitySuppressionSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        bridge_(std::make_unique<EntitySuppressionSyncBridge>(
            mock_processor_.CreateForwardingProcessor(),
            syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
                store_.get()))) {}

  EntitySuppressionSyncBridge& bridge() { return *bridge_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<EntitySuppressionSyncBridge> bridge_;
};

TEST_F(EntitySuppressionSyncBridgeTest, ClientTagAndStorageKey) {
  sync_pb::AutofillEntitySuppressionSpecifics spec =
      CreateTestSpecifics("guid-xyz");
  syncer::EntityData entity = EntityFromSpecifics(spec);

  EXPECT_EQ(bridge().GetClientTag(entity), "guid-xyz");
  EXPECT_EQ(bridge().GetStorageKey(entity), "guid-xyz");
  EXPECT_TRUE(bridge().IsEntityDataValid(entity));
}

}  // namespace

}  // namespace autofill
