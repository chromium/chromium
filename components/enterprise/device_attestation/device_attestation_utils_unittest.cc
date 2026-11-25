// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/device_attestation_utils.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/base64.h"
#include "crypto/hash.h"
#endif

namespace enterprise {

TEST(DeviceAttestationUtilsTest, GenerateV1ContentBindingString_EmptyReport) {
  enterprise_management::ChromeProfileReportRequest report;
  std::string json = GenerateV1ContentBindingString(report);
  EXPECT_EQ(json, R"({"version":1})");

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());
  EXPECT_EQ(dict->FindInt("version"), 1);
}

TEST(DeviceAttestationUtilsTest,
     GenerateV1ContentBindingString_WithDeviceAndProfileSignals) {
  enterprise_management::ChromeProfileReportRequest report;
  auto* device_id = report.mutable_browser_device_identifier();
  device_id->set_computer_name("computer_name");
#if BUILDFLAG(IS_CHROMEOS)
  device_id->set_serial_number("YX02T9XY");
#endif

  auto* os_report = report.mutable_os_report();
  os_report->set_device_enrollment_domain("example.com");
  os_report->set_device_manufacturer("Google");
  os_report->set_device_model("homestar");
  os_report->set_name("ChromeOS");
  os_report->set_version("16848.0.0");
#if BUILDFLAG(IS_CHROMEOS)
  os_report->set_disk_encryption(enterprise_management::SettingValue::ENABLED);
  os_report->set_os_firewall(enterprise_management::SettingValue::UNKNOWN);
  os_report->set_screen_lock_secured(
      enterprise_management::SettingValue::DISABLED);
#endif

  auto* browser_report = report.mutable_browser_report();
  browser_report->set_browser_version("157.0.0.0");
  auto* profile_info = browser_report->add_chrome_user_profile_infos();
  profile_info->set_profile_id("profile_123");
  auto* profile_signals = profile_info->mutable_profile_signals_report();
  profile_signals->set_built_in_dns_client_enabled(true);
  profile_signals->set_profile_enrollment_domain("example.com");
  profile_signals->set_realtime_url_check_mode(
      enterprise_management::ProfileSignalsReport::ENABLED_MAIN_FRAME);
  profile_signals->set_safe_browsing_protection_level(
      enterprise_management::ProfileSignalsReport::ENHANCED_PROTECTION);
  profile_signals->set_site_isolation_enabled(true);
  profile_signals->add_security_event_providers("crowdstrike");

  std::string json = GenerateV1ContentBindingString(report);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());
  EXPECT_EQ(*dict->FindString("display_name"), "computer_name");
  EXPECT_EQ(*dict->FindString("device_enrollment_domain"), "example.com");
  EXPECT_EQ(*dict->FindString("device_manufacturer"), "Google");
  EXPECT_EQ(*dict->FindString("device_model"), "homestar");
  EXPECT_EQ(*dict->FindString("operating_system"), "ChromeOS");
  EXPECT_EQ(*dict->FindString("os_version"), "16848.0.0");
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(*dict->FindString("serial_number"), "YX02T9XY");
  EXPECT_EQ(dict->FindInt("disk_encryption"), 2);
  EXPECT_EQ(dict->FindInt("os_firewall"), 0);
  EXPECT_EQ(dict->FindInt("screen_lock_secured"), 1);
  EXPECT_EQ(
      json,
      R"({"browser_version":"157.0.0.0","built_in_dns_client_enabled":true,"device_enrollment_domain":"example.com","device_manufacturer":"Google","device_model":"homestar","disk_encryption":2,"display_name":"computer_name","operating_system":"ChromeOS","os_firewall":0,"os_version":"16848.0.0","profile_enrollment_domain":"example.com","profile_id":"profile_123","realtime_url_check_mode":1,"safe_browsing_protection_level":2,"screen_lock_secured":1,"security_event_providers":["crowdstrike"],"serial_number":"YX02T9XY","site_isolation_enabled":true,"version":1})");
  EXPECT_EQ(base::Base64Encode(crypto::hash::Sha256(json)),
            "jcCGcQLYEBHSPScFSriEokJ8Y3eLd8F47IejkjzYwYc=");
#endif
  EXPECT_EQ(*dict->FindString("browser_version"), "157.0.0.0");
  EXPECT_EQ(*dict->FindString("profile_id"), "profile_123");
  EXPECT_EQ(dict->FindBool("built_in_dns_client_enabled"), true);
  EXPECT_EQ(*dict->FindString("profile_enrollment_domain"), "example.com");
  EXPECT_EQ(dict->FindBool("site_isolation_enabled"), true);
  const base::ListValue* providers = dict->FindList("security_event_providers");
  ASSERT_NE(providers, nullptr);
  ASSERT_EQ(providers->size(), 1u);
  EXPECT_EQ((*providers)[0].GetString(), "crowdstrike");
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(DeviceAttestationUtilsTest,
     GenerateV1ContentBindingString_ChromeOSPartialAndUnsetSignals) {
  enterprise_management::ChromeProfileReportRequest report;
  auto* device_id = report.mutable_browser_device_identifier();
  device_id->set_computer_name("computer_name");
  device_id->set_serial_number("");

  auto* os_report = report.mutable_os_report();
  os_report->set_name("ChromeOS");
  os_report->set_os_firewall(enterprise_management::SettingValue::ENABLED);

  std::string json = GenerateV1ContentBindingString(report);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());
  EXPECT_EQ(dict->FindString("serial_number"), nullptr);
  EXPECT_FALSE(dict->FindInt("disk_encryption").has_value());
  EXPECT_EQ(dict->FindInt("os_firewall"), 2);
  EXPECT_FALSE(dict->FindInt("screen_lock_secured").has_value());
  EXPECT_EQ(
      json,
      R"({"display_name":"computer_name","operating_system":"ChromeOS","os_firewall":2,"version":1})");
  EXPECT_EQ(base::Base64Encode(crypto::hash::Sha256(json)),
            "oUdyZlS+iCbbzbPESpcyQQrE0Ul+v5ckZVXSAd2OhlA=");
}
#endif

}  // namespace enterprise
