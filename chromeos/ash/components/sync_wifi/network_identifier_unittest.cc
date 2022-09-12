// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/test_data_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::sync_wifi {

namespace {
const char kHexSsid[] = "0123456789ABCDEF";
}  // namespace

class NetworkIdentifierTest : public testing::Test {
 public:
  NetworkIdentifierTest(const NetworkIdentifierTest&) = delete;
  NetworkIdentifierTest& operator=(const NetworkIdentifierTest&) = delete;

 protected:
  NetworkIdentifierTest() {}
};

TEST_F(NetworkIdentifierTest, FromProto) {
  NetworkIdentifier expected_id(kHexSsid, shill::kSecurityClassPsk);
  NetworkIdentifier id =
      NetworkIdentifier::FromProto(GenerateTestWifiSpecifics(expected_id));
  EXPECT_EQ(kHexSsid, id.hex_ssid());
  EXPECT_EQ(shill::kSecurityClassPsk, id.security_type());
  EXPECT_EQ(expected_id, id);
}

TEST_F(NetworkIdentifierTest, FromString) {
  std::string string_id("0123456789ABCDEF<||>psk");
  NetworkIdentifier id = NetworkIdentifier::DeserializeFromString(string_id);
  EXPECT_EQ(kHexSsid, id.hex_ssid());
  EXPECT_EQ(shill::kSecurityClassPsk, id.security_type());
}

TEST_F(NetworkIdentifierTest, DifferentHexFormats) {
  NetworkIdentifier id("0x2f", shill::kSecurityClassPsk);
  EXPECT_EQ("2F", id.hex_ssid());

  id = NetworkIdentifier("0X2F", shill::kSecurityClassPsk);
  EXPECT_EQ("2F", id.hex_ssid());

  id = NetworkIdentifier("2f", shill::kSecurityClassPsk);
  EXPECT_EQ("2F", id.hex_ssid());
}

TEST_F(NetworkIdentifierTest, Equality) {
  NetworkIdentifier first_id("0x2f", shill::kSecurityClassPsk);
  NetworkIdentifier second_id("0x2f", shill::kSecurityClassPsk);
  EXPECT_EQ(first_id, second_id);

  first_id = NetworkIdentifier("0x2f", shill::kSecurityClassPsk);
  second_id = NetworkIdentifier("0xff", shill::kSecurityClassPsk);
  EXPECT_NE(first_id, second_id);

  first_id = NetworkIdentifier("0x2f", shill::kSecurityClassPsk);
  second_id = NetworkIdentifier("0x2f", shill::kSecurityClassWep);
  EXPECT_NE(first_id, second_id);
}

TEST_F(NetworkIdentifierTest, Equality_InvalidNetworks) {
  NetworkIdentifier invalid_id("0x2f", "");
  NetworkIdentifier similar_invalid_id("0x2f", "");
  EXPECT_NE(invalid_id, similar_invalid_id);

  invalid_id = NetworkIdentifier("", shill::kSecurityClassPsk);
  similar_invalid_id = NetworkIdentifier("", shill::kSecurityClassPsk);
  EXPECT_NE(invalid_id, similar_invalid_id);

  invalid_id = NetworkIdentifier("", "");
  similar_invalid_id = NetworkIdentifier("", "");
  EXPECT_NE(invalid_id, similar_invalid_id);
}

}  // namespace ash::sync_wifi
