// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_service_impl.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_test_util.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_util.h"
#include "components/sync/base/features.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

using ::testing::Return;

class TestPlusAddressSettingSyncBridge : public PlusAddressSettingSyncBridge {
 public:
  using PlusAddressSettingSyncBridge::PlusAddressSettingSyncBridge;
  MOCK_METHOD(std::optional<sync_pb::PlusAddressSettingSpecifics>,
              GetSetting,
              (std::string_view),
              (const, override));
  MOCK_METHOD(void,
              WriteSetting,
              (const sync_pb::PlusAddressSettingSpecifics&),
              (override));
};

class PlusAddressSettingServiceImplTest : public testing::Test {
 public:
  PlusAddressSettingServiceImplTest() {
    auto bridge =
        std::make_unique<testing::NiceMock<TestPlusAddressSettingSyncBridge>>(
            mock_processor_.CreateForwardingProcessor(),
            /*store_factory=*/base::DoNothing());
    bridge_ = static_cast<TestPlusAddressSettingSyncBridge*>(bridge.get());
    service_ =
        std::make_unique<PlusAddressSettingServiceImpl>(std::move(bridge));
  }

  PlusAddressSettingService& service() { return *service_; }
  TestPlusAddressSettingSyncBridge& bridge() { return *bridge_; }

 private:
  base::test::ScopedFeatureList feature_{syncer::kSyncPlusAddressSetting};
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<PlusAddressSettingService> service_;
  raw_ptr<TestPlusAddressSettingSyncBridge> bridge_;  // Owned by the `service_`
};

// For settings that the client knows about, the correct values are returned.
TEST_F(PlusAddressSettingServiceImplTest, GetValue) {
  ON_CALL(bridge(), GetSetting("has_feature_enabled"))
      .WillByDefault(
          Return(CreateSettingSpecifics("has_feature_enabled", true)));
  EXPECT_TRUE(service().GetIsPlusAddressesEnabled());
}

// For settings that the client doesn't know about, setting-specific defaults
// are returned.
TEST_F(PlusAddressSettingServiceImplTest, GetValue_Defaults) {
  EXPECT_TRUE(service().GetIsPlusAddressesEnabled());
  EXPECT_FALSE(service().GetHasAcceptedNotice());
}

TEST_F(PlusAddressSettingServiceImplTest, SetValue) {
  EXPECT_CALL(bridge(),
              WriteSetting(HasBoolSetting("has_accepted_notice", true)));
  service().SetHasAcceptedNotice();
}

}  // namespace

}  // namespace plus_addresses
