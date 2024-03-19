// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include "base/test/task_environment.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {

class ProductSpecificationsSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    bridge_ = std::make_unique<ProductSpecificationsSyncBridge>(
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor());
  }

  ProductSpecificationsSyncBridge& bridge() { return *bridge_; }

 private:
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
};

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_compare()->set_uuid("my_uuid");
  EXPECT_EQ("my_uuid", bridge().GetStorageKey(entity));
}

TEST_F(ProductSpecificationsSyncBridgeTest, TestGetClientTag) {
  syncer::EntityData entity;
  entity.specifics.mutable_compare()->set_uuid("another_uuid");
  EXPECT_EQ("another_uuid", bridge().GetClientTag(entity));
}

}  // namespace commerce
