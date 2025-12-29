// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/cablev2_devices.h"

#include <memory>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/cable/pairing.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "testing/gtest/include/gtest/gtest.h"

using cablev2::KnownDevices;
using device::cablev2::Pairing;

namespace {

enum class Channel {
  UNKNOWN = 0,
  STABLE = 1,
  BETA = 2,
  DEV = 3,
  CANARY = 4,
  DEVELOPER = 5,
};

class CableV2DevicesProfileTest : public testing::Test {
  // A `BrowserTaskEnvironment` needs to be in-scope in order to create a
  // `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CableV2DevicesProfileTest, InitiallyEmpty) {
  TestingProfile profile;

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);

  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
}

struct TestDeviceInfoConfig {
  bool omit_paask_info = false;
  uint32_t id = device::cablev2::sync::IDNow();
  uint16_t tunnel_server_domain = 0;
};

syncer::DeviceInfo TestDeviceInfo(const TestDeviceInfoConfig& config) {
  syncer::DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.contact_id = std::vector<uint8_t>({1, 2, 3});
  std::ranges::fill(paask_info.peer_public_key_x962, 0);
  paask_info.peer_public_key_x962[0] = 1;
  std::ranges::fill(paask_info.secret, 0);
  paask_info.secret[0] = 2;
  paask_info.id = config.id;
  paask_info.tunnel_server_domain = config.tunnel_server_domain;

  std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> paask_info_opt;
  if (!config.omit_paask_info) {
    paask_info_opt = paask_info;
  }

  return syncer::DeviceInfo(
      /*guid=*/"guid",
      /*client_name=*/"client_name",
      /*chrome_version=*/"chrome_version",
      /*sync_user_agent=*/"sync_user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      syncer::DeviceInfo::OsType::kLinux,
      syncer::DeviceInfo::FormFactor::kDesktop,
      /*signin_scoped_device_id=*/"signin_scoped_device_id",
      /*manufacturer_name=*/"manufacturer_name",
      /*model_name=*/"model_name",
      /*full_hardware_class=*/"full_hardware_class",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*pulse_interval=*/base::TimeDelta(),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt, paask_info_opt,
      /*fcm_registration_token=*/"fcm_token", syncer::DataTypeSet(),
      /*auto_sign_out_last_signin_timestamp=*/base::Time::Now(),
      /*desktop_to_ios_promo_receiving_enabled=*/false);
}

TEST(CableV2FromSyncInfoTest, Basic) {
  TestDeviceInfoConfig config;
  syncer::DeviceInfo info = TestDeviceInfo(config);

  std::unique_ptr<device::cablev2::Pairing> pairing =
      cablev2::PairingFromSyncedDevice(&info, base::Time::Now());
  ASSERT_TRUE(pairing);
  EXPECT_TRUE(pairing->from_sync_deviceinfo);
  EXPECT_EQ(pairing->name, "client_name");
  EXPECT_EQ(base::HexEncode(pairing->contact_id), "010203");
  EXPECT_EQ(base::HexEncode(pairing->peer_public_key_x962),
            "010000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000");
  EXPECT_EQ(base::HexEncode(pairing->secret),
            "0200000000000000000000000000000000000000000000000000000000000000");
  EXPECT_EQ(pairing->channel_priority, 0);
}

TEST(CableV2FromSyncInfoTest, TooOld) {
  TestDeviceInfoConfig config;
  config.id -= device::cablev2::kMaxSyncInfoDaysForConsumer + 1;

  syncer::DeviceInfo info = TestDeviceInfo(config);

  std::unique_ptr<device::cablev2::Pairing> pairing =
      cablev2::PairingFromSyncedDevice(&info, base::Time::Now());
  EXPECT_FALSE(pairing);
}

TEST(CableV2FromSyncInfoTest, NoInfo) {
  TestDeviceInfoConfig config;
  config.omit_paask_info = true;
  syncer::DeviceInfo info = TestDeviceInfo(config);

  std::unique_ptr<device::cablev2::Pairing> pairing =
      cablev2::PairingFromSyncedDevice(&info, base::Time::Now());
  EXPECT_FALSE(pairing);
}

TEST(CableV2FromSyncInfoTest, UnknownTunnelServer) {
  TestDeviceInfoConfig config;
  config.tunnel_server_domain = 42;
  syncer::DeviceInfo info = TestDeviceInfo(config);

  std::unique_ptr<device::cablev2::Pairing> pairing =
      cablev2::PairingFromSyncedDevice(&info, base::Time::Now());
  EXPECT_FALSE(pairing);
}

}  // namespace
