// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_sync_bridge.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/plus_address_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

class PlusAddressSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    db_backend_ = base::MakeRefCounted<WebDatabaseBackend>(
        base::FilePath(WebDatabase::kInMemoryPath), /*delegate=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    db_backend_->AddTable(std::make_unique<PlusAddressTable>());
    db_backend_->InitDatabase();
    bridge_ = std::make_unique<PlusAddressSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), db_backend_);
  }

  PlusAddressSyncBridge& bridge() { return *bridge_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<WebDatabaseBackend> db_backend_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<PlusAddressSyncBridge> bridge_;
};

TEST_F(PlusAddressSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData entity;
  sync_pb::PlusAddressSpecifics* specifics =
      entity.specifics.mutable_plus_address();
  // Missing a profile ID.
  EXPECT_FALSE(bridge().IsEntityDataValid(entity));
  specifics->set_profile_id(123);
  EXPECT_TRUE(bridge().IsEntityDataValid(entity));
}

TEST_F(PlusAddressSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_plus_address()->set_profile_id(123);
  EXPECT_EQ(bridge().GetStorageKey(entity), "123");
}

}  // namespace

}  // namespace plus_addresses
