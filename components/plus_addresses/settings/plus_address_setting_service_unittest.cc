// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_service.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_test_util.h"
#include "components/sync/base/features.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

class PlusAddressSettingServiceTest : public testing::Test {
 public:
  PlusAddressSettingServiceTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    RecreateServiceWithSpecifics({});
  }

  PlusAddressSettingService& service() { return *service_; }

  // Simulates creating a service that is aware of the given `specifics`. It
  // does so by injecting the `specifics` into the store used by service's
  // sync bridge.
  void RecreateServiceWithSpecifics(
      const std::vector<sync_pb::PlusAddressSettingSpecifics>& specifics) {
    store_->DeleteAllDataAndMetadata(base::DoNothing());
    auto batch = store_->CreateWriteBatch();
    for (const sync_pb::PlusAddressSettingSpecifics& specific : specifics) {
      batch->WriteData(specific.name(), specific.SerializeAsString());
    }
    store_->CommitWriteBatch(std::move(batch), base::DoNothing());
    service_ = std::make_unique<PlusAddressSettingService>(
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
            store_.get()));
    // Wait for the `service_`'s initialisation to finish.
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_{syncer::kSyncPlusAddressSetting};
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<PlusAddressSettingService> service_;
};

TEST_F(PlusAddressSettingServiceTest, GetValue) {
  RecreateServiceWithSpecifics(
      {CreateSettingSpecifics("plus_address.is_enabled", true),
       CreateSettingSpecifics("plus_address.has_accepted_notice", false)});

  // For settings that the client knows about, the correct values are returned.
  EXPECT_TRUE(service().GetIsPlusAddressesEnabled());
  EXPECT_FALSE(service().GetHasAcceptedNotice());
  // For settings that the client hasn't received, defaults are returned.
  EXPECT_FALSE(service().GetIsOptedInToDogfood());
}

}  // namespace

}  // namespace plus_addresses
