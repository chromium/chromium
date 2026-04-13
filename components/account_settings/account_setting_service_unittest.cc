// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_settings/account_setting_service_impl.h"
#include "components/account_settings/account_setting_sync_bridge.h"
#include "components/account_settings/account_setting_sync_util.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/account_setting_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace account_settings {

namespace {

using ::testing::Eq;
using ::testing::Optional;
using ::testing::Return;

class MockAccountSettingSyncBridge : public AccountSettingSyncBridge {
 public:
  using AccountSettingSyncBridge::AccountSettingSyncBridge;
  MOCK_METHOD(base::Value, GetSetting, (std::string_view), (const, override));
  MOCK_METHOD(std::optional<bool>,
              GetBooleanSetting,
              (std::string_view),
              (const, override));
  MOCK_METHOD(std::optional<int>,
              GetIntSetting,
              (std::string_view),
              (const, override));
  MOCK_METHOD(std::optional<std::string>,
              GetStringSetting,
              (std::string_view),
              (const, override));
};

class AccountSettingServiceTest : public testing::Test {
 public:
  AccountSettingServiceTest() {
    auto bridge =
        std::make_unique<testing::NiceMock<MockAccountSettingSyncBridge>>(
            mock_processor_.CreateForwardingProcessor(),
            /*store_factory=*/base::DoNothing());
    bridge_ = static_cast<MockAccountSettingSyncBridge*>(bridge.get());
    service_ = std::make_unique<AccountSettingServiceImpl>(std::move(bridge));
  }

  AccountSettingService& service() { return *service_; }
  MockAccountSettingSyncBridge& bridge() { return *bridge_; }

 private:
  base::test::ScopedFeatureList feature_{syncer::kSyncAccountSettings};
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<AccountSettingServiceImpl> service_;
  raw_ptr<MockAccountSettingSyncBridge> bridge_;  // Owned by the `service_`
};

TEST_F(AccountSettingServiceTest, GetBoolean) {
  EXPECT_THAT(service().GetBoolean(kWalletPrivacyContextualSurfacing),
              Eq(std::nullopt));
  ON_CALL(bridge(), GetBooleanSetting("WALLET_PRIVACY_CONTEXTUAL_SURFACING"))
      .WillByDefault(Return(true));
  EXPECT_THAT(service().GetBoolean(kWalletPrivacyContextualSurfacing),
              Optional(true));
}

TEST_F(AccountSettingServiceTest, GetBooleanReturnsNulloptIfFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kSyncAccountSettings);

  EXPECT_THAT(service().GetBoolean(kWalletPrivacyContextualSurfacing),
              Eq(std::nullopt));
}

}  // namespace

}  // namespace account_settings
