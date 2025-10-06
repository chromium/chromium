// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_util.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/account_setting_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class AccountSettingSyncBridgeTest : public testing::Test {
 public:
  AccountSettingSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    RecreateBridge();
  }

  void RecreateBridge() {
    bridge_ = std::make_unique<AccountSettingSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
  }

  AccountSettingSyncBridge& bridge() { return *bridge_; }

  syncer::DataTypeStore& store() { return *store_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<AccountSettingSyncBridge> bridge_;
};

TEST_F(AccountSettingSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity;
  *entity.specifics.mutable_account_setting() =
      CreateSettingSpecifics("name", "value");
  EXPECT_EQ(bridge().GetStorageKey(entity), "name");
  // `GetClientTag()` is implemented using `GetStorageKey()`.
  EXPECT_EQ(bridge().GetClientTag(entity), "name");
}

}  // namespace

}  // namespace autofill
