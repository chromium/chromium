// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

#include <memory>
#include <string>

#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::network_config {

class CrosNetworkConfigUtilTest : public testing::Test {
 public:
  CrosNetworkConfigUtilTest() = default;
  CrosNetworkConfigUtilTest(const CrosNetworkConfigUtilTest&) = delete;
  CrosNetworkConfigUtilTest& operator=(const CrosNetworkConfigUtilTest&) =
      delete;
  ~CrosNetworkConfigUtilTest() override = default;
};

TEST_F(CrosNetworkConfigUtilTest, WifiConfigConversion) {
  auto wifi_config = mojom::WiFiConfigProperties::New();

  const std::string kTestSsid("TEST_SSID");
  wifi_config->ssid = kTestSsid;

  int kTestSecurity = static_cast<int>(mojom::SecurityType::kWpaPsk);
  wifi_config->security = static_cast<mojom::SecurityType>(kTestSecurity);

  const std::string kTestPassphrase("TEST_PASSPHRASE");
  wifi_config->passphrase = kTestPassphrase;

  // Fill EAP config.
  auto eap_config = mojom::EAPConfigProperties::New();

  const std::string kEapOuter = onc::eap::kEAP_TTLS;
  eap_config->outer = kEapOuter;
  const std::string kEapInner = onc::eap::kMSCHAPv2;
  eap_config->inner = kEapInner;
  const std::string kEapIdentity("TEST_IDENTITY");
  eap_config->identity = kEapIdentity;
  const std::string kEapAnonymousIdentity("TEST_ANONYMOUS_IDENTITY");
  eap_config->anonymous_identity = kEapAnonymousIdentity;
  eap_config->password = kTestPassphrase;
  wifi_config->eap = std::move(eap_config);

  base::Value::Dict dict = WiFiConfigPropertiesToMojoJsValue(wifi_config);

  auto* ssid = dict.FindStringByDottedPath("typeConfig.wifi.ssid");
  EXPECT_NE(ssid, nullptr);
  EXPECT_EQ(*ssid, kTestSsid);

  auto security = dict.FindIntByDottedPath("typeConfig.wifi.security");
  EXPECT_NE(security, std::nullopt);
  EXPECT_EQ(*security, kTestSecurity);

  auto* passphrase = dict.FindStringByDottedPath("typeConfig.wifi.passphrase");
  EXPECT_NE(passphrase, nullptr);
  EXPECT_EQ(*passphrase, kTestPassphrase);

  auto* eap_outer = dict.FindStringByDottedPath("typeConfig.wifi.eap.outer");
  EXPECT_NE(eap_outer, nullptr);
  EXPECT_EQ(*eap_outer, kEapOuter);

  auto* eap_inner = dict.FindStringByDottedPath("typeConfig.wifi.eap.inner");
  EXPECT_NE(eap_inner, nullptr);
  EXPECT_EQ(*eap_inner, kEapInner);

  auto* eap_identity =
      dict.FindStringByDottedPath("typeConfig.wifi.eap.identity");
  EXPECT_NE(eap_identity, nullptr);
  EXPECT_EQ(*eap_identity, kEapIdentity);

  auto* eap_anonymous_identity =
      dict.FindStringByDottedPath("typeConfig.wifi.eap.anonymousIdentity");
  EXPECT_NE(eap_anonymous_identity, nullptr);
  EXPECT_EQ(*eap_anonymous_identity, kEapAnonymousIdentity);

  auto* eap_password =
      dict.FindStringByDottedPath("typeConfig.wifi.eap.password");
  EXPECT_NE(eap_password, nullptr);
  EXPECT_EQ(*eap_password, kTestPassphrase);
}

}  // namespace chromeos::network_config
