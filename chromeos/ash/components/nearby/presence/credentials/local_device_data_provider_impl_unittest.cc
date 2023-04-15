// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

const std::string kUserEmail = "test.tester@gmail.com";
const std::string kCanocalizedUserEmail = "testtester@gmail.com";
const std::string kGivenName = "Test";
const std::string kUserName = "Test Tester";
const std::string kProfileUrl = "https://example.com";

}  // namespace

namespace ash::nearby::presence {

class LocalDeviceDataProviderImplTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterNearbyPresenceCredentialPrefs(pref_service_.registry());
    account_info_ = identity_test_env_.MakePrimaryAccountAvailable(
        kUserEmail, signin::ConsentLevel::kSignin);
  }

  void CreateDataProvider() {
    local_device_data_provider_ = std::make_unique<LocalDeviceDataProviderImpl>(
        &pref_service_, identity_test_env_.identity_manager());
  }

  void DestroyDataProvider() { local_device_data_provider_.reset(); }

 protected:
  AccountInfo account_info_;
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;
};

TEST_F(LocalDeviceDataProviderImplTest, DeviceId) {
  CreateDataProvider();

  // A 10-character alphanumeric ID is automatically generated if one doesn't
  // already exist.
  std::string id = local_device_data_provider_->GetDeviceId();
  EXPECT_EQ(10u, id.size());
  for (const char c : id) {
    EXPECT_TRUE(std::isalnum(c));
  }

  // The ID is persisted.
  DestroyDataProvider();
  CreateDataProvider();
  EXPECT_EQ(id, local_device_data_provider_->GetDeviceId());
}

TEST_F(LocalDeviceDataProviderImplTest, AccountName) {
  CreateDataProvider();
  EXPECT_EQ(kCanocalizedUserEmail,
            local_device_data_provider_->GetAccountName());
}

TEST_F(LocalDeviceDataProviderImplTest, Metadata) {
  CreateDataProvider();

  // Assume that first time registration has already occurred.
  pref_service_.SetString(prefs::kNearbyPresenceUserNamePrefName, kUserName);
  pref_service_.SetString(prefs::kNearbyPresenceProfileUrlPrefName,
                          kProfileUrl);

  // Assume that without the given name, the device name is just the
  // device type.
  ::nearby::internal::Metadata metadata =
      local_device_data_provider_->GetDeviceMetadata();
  EXPECT_EQ(base::UTF16ToUTF8(ui::GetChromeOSDeviceName()),
            metadata.device_name());

  // Populate the account information and expect the device name to include the
  // user's given name.
  account_info_.given_name = kGivenName;
  identity_test_env_.UpdateAccountInfoForAccount(account_info_);
  metadata = local_device_data_provider_->GetDeviceMetadata();
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_NEARBY_PRESENCE_DEVICE_NAME,
                                      base::UTF8ToUTF16(kGivenName),
                                      ui::GetChromeOSDeviceName()),
            metadata.device_name());

  // Confirm the other fields are expected.
  EXPECT_EQ(kCanocalizedUserEmail, metadata.account_name());
  EXPECT_EQ(kUserName, metadata.user_name());
  EXPECT_EQ(kProfileUrl, metadata.device_profile_url());
  EXPECT_EQ(std::string(), metadata.bluetooth_mac_address());
}

}  // namespace ash::nearby::presence
