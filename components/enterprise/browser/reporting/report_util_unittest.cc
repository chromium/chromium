// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_util.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

namespace {

em::ChromeProfileReportRequest GetTestReportRequest() {
  em::ChromeProfileReportRequest request;

  auto* device_id = request.mutable_browser_device_identifier();
  device_id->set_computer_name("computer_name");
  device_id->set_serial_number("serial_number");
  device_id->set_host_name("hostname");

  auto* os_report = request.mutable_os_report();
  os_report->set_device_enrollment_domain("test.com");
  os_report->set_device_manufacturer("manufacturer");
  os_report->set_device_model("model");
  os_report->set_disk_encryption(em::SettingValue::ENABLED);
  os_report->add_mac_addresses("mac_address_1");
  os_report->add_mac_addresses("mac_address_2");
  os_report->set_name("Windows");
  os_report->set_os_firewall(em::SettingValue::DISABLED);
  os_report->set_screen_lock_secured(em::SettingValue::UNKNOWN);
  os_report->add_system_dns_servers("1.2.3.4");
  os_report->add_system_dns_servers("5.6.7.8");
  os_report->set_version("10.0.19045");

#if BUILDFLAG(IS_ANDROID)
  os_report->set_has_potentially_harmful_apps(false);
  os_report->set_verified_apps_enabled(true);
  os_report->set_security_patch_ms(1735718400000LL);
#endif

  auto* browser_report = request.mutable_browser_report();
  browser_report->set_browser_version("149.0.7797.0");

  auto* profile_info = browser_report->add_chrome_user_profile_infos();
  profile_info->set_profile_id("profile_id");

  auto* profile_signals = profile_info->mutable_profile_signals_report();
  profile_signals->set_built_in_dns_client_enabled(true);
  profile_signals->set_chrome_remote_desktop_app_blocked(false);
  profile_signals->set_password_protection_warning_trigger(
      em::ProfileSignalsReport::PASSWORD_REUSE);
  profile_signals->set_profile_enrollment_domain("test2.com");
  profile_signals->set_realtime_url_check_mode(
      em::ProfileSignalsReport::ENABLED_MAIN_FRAME);
  profile_signals->set_safe_browsing_protection_level(
      em::ProfileSignalsReport::ENHANCED_PROTECTION);
  profile_signals->set_site_isolation_enabled(true);

  profile_signals->add_file_downloaded_providers("provider1");
  profile_signals->add_file_downloaded_providers("provider2");
  profile_signals->add_bulk_data_entry_providers("provider3");
  profile_signals->add_bulk_data_entry_providers("provider4");
  profile_signals->add_bulk_data_entry_providers("provider5");
  profile_signals->add_print_providers("provider6");
  profile_signals->add_print_providers("provider7");
  profile_signals->add_security_event_providers("provider9");

  return request;
}

}  // namespace



// Tests that an empty report request only returns the current version.
TEST(ReportUtilTest, GenerateV1ContentBindingString_EmptyReport) {
  em::ChromeProfileReportRequest request;
  EXPECT_EQ(GenerateV1ContentBindingString(request), R"({"version":1})");
}

// Tests that a fully populated report request maps correctly to the JSON
// output.
TEST(ReportUtilTest, GenerateV1ContentBindingString_PopulatedReport) {
  // DISCLAIMER: If you are updating this test it is because you have modified
  // the version of our report. Any changes should be done behind a flag and
  // reflected on the server.
  std::string json_result =
      GenerateV1ContentBindingString(GetTestReportRequest());

#if BUILDFLAG(IS_ANDROID)
  const char kExpectedJson[] =
      R"({"browser_version":"149.0.7797.0","built_in_dns_client_enabled":true,"device_enrollment_domain":"test.com","device_manufacturer":"manufacturer","device_model":"model","display_name":"computer_name","has_potentially_harmful_apps":false,"operating_system":"Windows","os_version":"10.0.19045","profile_enrollment_domain":"test2.com","profile_id":"profile_id","realtime_url_check_mode":1,"safe_browsing_protection_level":2,"security_event_providers":["provider9"],"security_patch_ms":"1735718400000","site_isolation_enabled":true,"verified_apps_enabled":true,"version":1})";
#else
  const char kExpectedJson[] =
      R"({"browser_version":"149.0.7797.0","built_in_dns_client_enabled":true,"device_enrollment_domain":"test.com","device_manufacturer":"manufacturer","device_model":"model","display_name":"computer_name","operating_system":"Windows","os_version":"10.0.19045","profile_enrollment_domain":"test2.com","profile_id":"profile_id","realtime_url_check_mode":1,"safe_browsing_protection_level":2,"security_event_providers":["provider9"],"site_isolation_enabled":true,"version":1})";
#endif

  EXPECT_EQ(json_result, kExpectedJson);
}

}  // namespace enterprise_reporting
