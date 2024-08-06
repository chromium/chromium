// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_provider_impl.h"

#include <optional>

#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/version_info/version_string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

const char kLocalDeviceGuid[] = "foo";
const char kLocalDeviceClientName[] = "bar";
const char kLocalDeviceManufacturerName[] = "manufacturer";
const char kLocalDeviceModelName[] = "model";
const char kLocalFullHardwareClass[] = "test_full_hardware_class";

const char kSharingVapidFCMRegistrationToken[] = "test_vapid_fcm_token";
const char kSharingVapidP256dh[] = "test_vapid_p256_dh";
const char kSharingVapidAuthSecret[] = "test_vapid_auth_secret";
const char kSharingSenderIdFCMRegistrationToken[] = "test_sender_id_fcm_token";
const char kSharingSenderIdP256dh[] = "test_sender_id_p256_dh";
const char kSharingSenderIdAuthSecret[] = "test_sender_id_auth_secret";
const char kSharingChimeRepresentativeTargetId[] =
    "chime_representative_target_id";
const sync_pb::SharingSpecificFields::EnabledFeatures
    kSharingEnabledFeatures[] = {
        sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2};

using testing::NiceMock;
using testing::NotNull;
using testing::Return;

class MockDeviceInfoSyncClient : public DeviceInfoSyncClient {
 public:
  MockDeviceInfoSyncClient() = default;

  MockDeviceInfoSyncClient(const MockDeviceInfoSyncClient&) = delete;
  MockDeviceInfoSyncClient& operator=(const MockDeviceInfoSyncClient&) = delete;

  ~MockDeviceInfoSyncClient() override = default;

  MOCK_METHOD(std::string, GetSigninScopedDeviceId, (), (const override));
  MOCK_METHOD(bool, GetSendTabToSelfReceivingEnabled, (), (const override));
  MOCK_METHOD(sync_pb::SyncEnums_SendTabReceivingType,
              GetSendTabToSelfReceivingType,
              (),
              (const override));
  MOCK_METHOD(std::optional<DeviceInfo::SharingInfo>,
              GetLocalSharingInfo,
              (),
              (const override));
  MOCK_METHOD(DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo,
              GetPhoneAsASecurityKeyInfo,
              (),
              (const override));
  MOCK_METHOD(std::optional<std::string>,
              GetFCMRegistrationToken,
              (),
              (const override));
  MOCK_METHOD(std::optional<DataTypeSet>,
              GetInterestedDataTypes,
              (),
              (const override));
  MOCK_METHOD(bool, IsUmaEnabledOnCrOSDevice, (), (const override));
};

class LocalDeviceInfoProviderImplTest : public testing::Test {
 public:
  LocalDeviceInfoProviderImplTest() = default;
  ~LocalDeviceInfoProviderImplTest() override = default;

  void SetUp() override {
    provider_ = std::make_unique<LocalDeviceInfoProviderImpl>(
        version_info::Channel::UNKNOWN,
        version_info::GetVersionStringWithModifier("UNKNOWN"),
        &device_info_sync_client_);
  }

  void TearDown() override { provider_.reset(); }

 protected:
  void InitializeProvider() { InitializeProvider(kLocalDeviceGuid); }

  void InitializeProvider(const std::string& guid) {
    provider_->Initialize(guid, kLocalDeviceClientName,
                          kLocalDeviceManufacturerName, kLocalDeviceModelName,
                          kLocalFullHardwareClass,
                          /*device_info_restored_from_store=*/nullptr);
  }

  DeviceInfo::PhoneAsASecurityKeyInfo SamplePhoneAsASecurityKeyInfo() {
    DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
    paask_info.tunnel_server_domain = 123;
    paask_info.contact_id = {1, 2, 3, 4};
    paask_info.secret = {5, 6, 7, 8};
    paask_info.id = 321;
    paask_info.peer_public_key_x962 = {10, 11, 12, 13};
    return paask_info;
  }

  testing::NiceMock<MockDeviceInfoSyncClient> device_info_sync_client_;
  std::unique_ptr<LocalDeviceInfoProviderImpl> provider_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(LocalDeviceInfoProviderImplTest, UmaToggleFullHardwareClass) {
  InitializeProvider(kLocalDeviceGuid);

  // Tests that |full_hardware_class| maintains correct values on toggling UMA
  // from ON -> OFF, OFF -> ON
  ON_CALL(device_info_sync_client_, IsUmaEnabledOnCrOSDevice)
      .WillByDefault(Return(true));
  EXPECT_EQ(provider_->GetLocalDeviceInfo()->full_hardware_class(),
            kLocalFullHardwareClass);

  ON_CALL(device_info_sync_client_, IsUmaEnabledOnCrOSDevice)
      .WillByDefault(Return(false));
  EXPECT_EQ(provider_->GetLocalDeviceInfo()->full_hardware_class(), "");

  ON_CALL(device_info_sync_client_, IsUmaEnabledOnCrOSDevice)
      .WillByDefault(Return(true));
  EXPECT_EQ(provider_->GetLocalDeviceInfo()->full_hardware_class(),
            kLocalFullHardwareClass);
}
#else   // NOT BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(LocalDeviceInfoProviderImplTest,
       UmaEnabledNonChromeOSHardwareClassEmpty) {
  // Tests that the |full_hardware_class| doesn't get updated when on
  // non-chromeos device. IsUmaEnabledOnCrOSDevice() returns false on non-cros.
  ON_CALL(device_info_sync_client_, IsUmaEnabledOnCrOSDevice)
      .WillByDefault(Return(false));

  InitializeProvider(kLocalDeviceGuid);

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();

  // |kLocalFullHardwareClass| is reset after retrieving |local_device_info|
  EXPECT_EQ(local_device_info->full_hardware_class(), "");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(LocalDeviceInfoProviderImplTest, GetLocalDeviceInfo) {
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());

  InitializeProvider();

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();
  ASSERT_NE(nullptr, local_device_info);
  EXPECT_EQ("", local_device_info->full_hardware_class());
  EXPECT_EQ(std::string(kLocalDeviceGuid), local_device_info->guid());
  EXPECT_EQ(kLocalDeviceClientName, local_device_info->client_name());
  EXPECT_EQ(kLocalDeviceManufacturerName,
            local_device_info->manufacturer_name());
  EXPECT_EQ(kLocalDeviceModelName, local_device_info->model_name());
  EXPECT_EQ(MakeUserAgentForSync(provider_->GetChannel()),
            local_device_info->sync_user_agent());

  provider_->Clear();
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());
}

TEST_F(LocalDeviceInfoProviderImplTest, GetSigninScopedDeviceId) {
  const std::string kSigninScopedDeviceId = "device_id";

  EXPECT_CALL(device_info_sync_client_, GetSigninScopedDeviceId())
      .WillOnce(Return(kSigninScopedDeviceId));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_EQ(kSigninScopedDeviceId,
            provider_->GetLocalDeviceInfo()->signin_scoped_device_id());
}

TEST_F(LocalDeviceInfoProviderImplTest, SendTabToSelfReceivingEnabled) {
  ON_CALL(device_info_sync_client_, GetSendTabToSelfReceivingEnabled())
      .WillByDefault(Return(true));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_TRUE(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_enabled());

  ON_CALL(device_info_sync_client_, GetSendTabToSelfReceivingEnabled())
      .WillByDefault(Return(false));

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_FALSE(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_enabled());
}

TEST_F(LocalDeviceInfoProviderImplTest, SendTabToSelfReceivingType) {
  ON_CALL(device_info_sync_client_, GetSendTabToSelfReceivingType())
      .WillByDefault(Return(
          sync_pb::
              SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_EQ(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_type(),
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED);

  ON_CALL(device_info_sync_client_, GetSendTabToSelfReceivingType())
      .WillByDefault(Return(
          sync_pb::
              SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION));

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_EQ(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_type(),
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION);
}

TEST_F(LocalDeviceInfoProviderImplTest, SharingInfo) {
  ON_CALL(device_info_sync_client_, GetLocalSharingInfo())
      .WillByDefault(Return(std::nullopt));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_FALSE(provider_->GetLocalDeviceInfo()->sharing_info());

  std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features(
      std::begin(kSharingEnabledFeatures), std::end(kSharingEnabledFeatures));
  std::optional<DeviceInfo::SharingInfo> sharing_info =
      std::make_optional<DeviceInfo::SharingInfo>(
          DeviceInfo::SharingTargetInfo{kSharingVapidFCMRegistrationToken,
                                        kSharingVapidP256dh,
                                        kSharingVapidAuthSecret},
          DeviceInfo::SharingTargetInfo{kSharingSenderIdFCMRegistrationToken,
                                        kSharingSenderIdP256dh,
                                        kSharingSenderIdAuthSecret},
          kSharingChimeRepresentativeTargetId, enabled_features);
  ON_CALL(device_info_sync_client_, GetLocalSharingInfo())
      .WillByDefault(Return(sharing_info));

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  const std::optional<DeviceInfo::SharingInfo>& local_sharing_info =
      provider_->GetLocalDeviceInfo()->sharing_info();
  ASSERT_TRUE(local_sharing_info);
  EXPECT_EQ(kSharingVapidFCMRegistrationToken,
            local_sharing_info->vapid_target_info.fcm_token);
  EXPECT_EQ(kSharingVapidP256dh, local_sharing_info->vapid_target_info.p256dh);
  EXPECT_EQ(kSharingVapidAuthSecret,
            local_sharing_info->vapid_target_info.auth_secret);
  EXPECT_EQ(kSharingSenderIdFCMRegistrationToken,
            local_sharing_info->sender_id_target_info.fcm_token);
  EXPECT_EQ(kSharingSenderIdP256dh,
            local_sharing_info->sender_id_target_info.p256dh);
  EXPECT_EQ(kSharingChimeRepresentativeTargetId,
            local_sharing_info->chime_representative_target_id);
  EXPECT_EQ(kSharingSenderIdAuthSecret,
            local_sharing_info->sender_id_target_info.auth_secret);
  EXPECT_EQ(enabled_features, local_sharing_info->enabled_features);
}

TEST_F(LocalDeviceInfoProviderImplTest, ShouldPopulateFCMRegistrationToken) {
  InitializeProvider();
  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_TRUE(
      provider_->GetLocalDeviceInfo()->fcm_registration_token().empty());

  const std::string kFCMRegistrationToken = "token";
  EXPECT_CALL(device_info_sync_client_, GetFCMRegistrationToken())
      .WillRepeatedly(Return(kFCMRegistrationToken));

  EXPECT_EQ(provider_->GetLocalDeviceInfo()->fcm_registration_token(),
            kFCMRegistrationToken);
}

TEST_F(LocalDeviceInfoProviderImplTest, ShouldPopulateInterestedDataTypes) {
  InitializeProvider();
  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_TRUE(provider_->GetLocalDeviceInfo()->interested_data_types().empty());

  const DataTypeSet kTypes = {BOOKMARKS};
  EXPECT_CALL(device_info_sync_client_, GetInterestedDataTypes())
      .WillRepeatedly(Return(kTypes));

  EXPECT_EQ(provider_->GetLocalDeviceInfo()->interested_data_types(), kTypes);
}

TEST_F(LocalDeviceInfoProviderImplTest, ShouldKeepStoredInvalidationFields) {
  const std::string kFCMRegistrationToken = "fcm_token";
  const DataTypeSet kInterestedDataTypes = {BOOKMARKS};

  DeviceInfo::PhoneAsASecurityKeyInfo paask_info =
      SamplePhoneAsASecurityKeyInfo();
  const DeviceInfo device_info_restored_from_store(
      kLocalDeviceGuid, "name", "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, DeviceInfo::OsType::kLinux,
      DeviceInfo::FormFactor::kDesktop, "device_id", "manufacturer", "model",
      "full_hardware_class", base::Time(), base::Days(1),
      /*send_tab_to_self_receiving_enabled=*/
      true,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt, paask_info, kFCMRegistrationToken,
      kInterestedDataTypes,
      /*floating_workspace_last_signin_timestamp=*/std::nullopt);

  // |kFCMRegistrationToken|, |kInterestedDataTypes|,
  // and |paask_info| should be taken from |device_info_restored_from_store|
  // when |device_info_sync_client_| returns nullopt.
  provider_->Initialize(kLocalDeviceGuid, kLocalDeviceClientName,
                        kLocalDeviceManufacturerName, kLocalDeviceModelName,
                        kLocalFullHardwareClass,
                        &device_info_restored_from_store);

  EXPECT_CALL(device_info_sync_client_, GetFCMRegistrationToken())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(device_info_sync_client_, GetInterestedDataTypes())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(device_info_sync_client_, GetPhoneAsASecurityKeyInfo())
      .WillOnce(Return(DeviceInfo::PhoneAsASecurityKeyInfo::NotReady()));

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();
  EXPECT_EQ(local_device_info->interested_data_types(), kInterestedDataTypes);
  EXPECT_EQ(local_device_info->fcm_registration_token(), kFCMRegistrationToken);
  EXPECT_TRUE(
      local_device_info->paask_info()->NonRotatingFieldsEqual(paask_info));

  // `GetPhoneAsASecurityKeyInfo` can erase the field too.
  EXPECT_CALL(device_info_sync_client_, GetPhoneAsASecurityKeyInfo())
      .WillOnce(Return(DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport()));

  const DeviceInfo* local_device_info2 = provider_->GetLocalDeviceInfo();
  EXPECT_FALSE(local_device_info2->paask_info().has_value());
}

TEST_F(LocalDeviceInfoProviderImplTest, PhoneAsASecurityKeyInfo) {
  ON_CALL(device_info_sync_client_, GetPhoneAsASecurityKeyInfo())
      .WillByDefault(Return(DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport()));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_FALSE(provider_->GetLocalDeviceInfo()->paask_info());

  DeviceInfo::PhoneAsASecurityKeyInfo paask_info =
      SamplePhoneAsASecurityKeyInfo();
  ON_CALL(device_info_sync_client_, GetPhoneAsASecurityKeyInfo())
      .WillByDefault(Return(paask_info));

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  const std::optional<DeviceInfo::PhoneAsASecurityKeyInfo>& result_paask_info =
      provider_->GetLocalDeviceInfo()->paask_info();
  ASSERT_TRUE(result_paask_info);
  EXPECT_EQ(paask_info.tunnel_server_domain,
            result_paask_info->tunnel_server_domain);
  EXPECT_EQ(paask_info.contact_id, result_paask_info->contact_id);
  EXPECT_EQ(paask_info.secret, result_paask_info->secret);
  EXPECT_EQ(paask_info.id, result_paask_info->id);
  EXPECT_EQ(paask_info.peer_public_key_x962,
            result_paask_info->peer_public_key_x962);
}

}  // namespace
}  // namespace syncer
