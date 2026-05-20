// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/device_attestation_service_android.h"

#include <sys/mman.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/device_attestation/android/android_attestation_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise {

class MockAttestationClient : public AndroidAttestationClient {
 public:
  MOCK_METHOD(void,
              GenerateAttestationBlob,
              (std::string_view,
               std::string_view,
               std::string_view,
               std::string_view,
               base::OnceCallback<void(BlobGenerationResult)>),
              (override));
};

class DeviceAttestationServiceAndroidTest : public testing::Test {
 public:
  DeviceAttestationServiceAndroidTest() {
    auto client =
        std::make_unique<testing::StrictMock<MockAttestationClient>>();
    client_ = client.get();
    service_ =
        std::make_unique<DeviceAttestationServiceAndroid>(std::move(client));
  }
  ~DeviceAttestationServiceAndroidTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DeviceAttestationServiceAndroid> service_;
  raw_ptr<testing::StrictMock<MockAttestationClient>> client_;
};

// Tests that GetAttestationResponse safely copies `std::string_view` arguments,
// preventing a use-after-free crash when the original strings are destroyed
// before the background task executes.
TEST_F(DeviceAttestationServiceAndroidTest,
       GetAttestationResponse_DoesNotCrashOnTemporaryStrings) {
  base::RunLoop run_loop;
  enterprise_management::ChromeProfileReportRequest report;

  EXPECT_CALL(*client_,
              GenerateAttestationBlob(testing::_, testing::_, testing::_,
                                      testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<4>(
          BlobGenerationResult{.attestation_blob = "", .error_message = ""}));

  // Explicitly construct and pass a large temporary std::string object for
  // request payload only, to avoid Small String Optimization. This ensures it
  // to be allocated on the heap and destroyed at the end of this statement.
  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse(
      std::string("flow_name"), report,
      std::string_view(std::string(1024 * 1024, 'r')), std::string("timestamp"),
      std::string("nonce"), test_future.GetCallback());

  EXPECT_TRUE(test_future.Wait());
}

// This test validates the hashes that will be used for the content binding
// on Android when using a specific report.
TEST_F(DeviceAttestationServiceAndroidTest,
       GetAttestationResponse_WithContentBindingVersioning) {
  // DISCLAIMER: If you are updating this test it is because you have modified
  // the version of our report. Any changes should be done behind a flag and
  // reflected on the server.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      enterprise_signals::features::kContentBindingVersioningEnabled);

  enterprise_management::ChromeProfileReportRequest report;

  auto* device_id = report.mutable_browser_device_identifier();
  device_id->set_computer_name("computer_name");
  device_id->set_serial_number("serial_number");
  device_id->set_host_name("hostname");

  auto* os_report = report.mutable_os_report();
  os_report->set_device_enrollment_domain("test.com");
  os_report->set_device_manufacturer("manufacturer");
  os_report->set_device_model("model");
  os_report->set_disk_encryption(enterprise_management::SettingValue::ENABLED);
  os_report->add_mac_addresses("mac_address_1");
  os_report->add_mac_addresses("mac_address_2");
  os_report->set_name("Android");
  os_report->set_os_firewall(enterprise_management::SettingValue::DISABLED);
  os_report->set_screen_lock_secured(
      enterprise_management::SettingValue::UNKNOWN);
  os_report->add_system_dns_servers("1.2.3.4");
  os_report->add_system_dns_servers("5.6.7.8");
  os_report->set_version("10.0.19045");
  os_report->set_has_potentially_harmful_apps(false);
  os_report->set_verified_apps_enabled(true);
  os_report->set_security_patch_ms(1735718400000L);

  auto* browser_report = report.mutable_browser_report();
  browser_report->set_browser_version("149.0.7797.0");

  auto* profile_info = browser_report->add_chrome_user_profile_infos();
  profile_info->set_profile_id("profile_id");

  auto* profile_signals = profile_info->mutable_profile_signals_report();
  profile_signals->set_built_in_dns_client_enabled(true);
  profile_signals->set_chrome_remote_desktop_app_blocked(false);
  profile_signals->set_password_protection_warning_trigger(
      enterprise_management::ProfileSignalsReport::PASSWORD_REUSE);
  profile_signals->set_profile_enrollment_domain("test2.com");
  profile_signals->set_realtime_url_check_mode(
      enterprise_management::ProfileSignalsReport::ENABLED_MAIN_FRAME);
  profile_signals->set_safe_browsing_protection_level(
      enterprise_management::ProfileSignalsReport::ENHANCED_PROTECTION);
  profile_signals->set_site_isolation_enabled(true);
  profile_signals->add_file_downloaded_providers("provider1");
  profile_signals->add_file_downloaded_providers("provider2");
  profile_signals->add_bulk_data_entry_providers("provider3");
  profile_signals->add_bulk_data_entry_providers("provider4");
  profile_signals->add_bulk_data_entry_providers("provider5");
  profile_signals->add_print_providers("provider6");
  profile_signals->add_print_providers("provider7");
  profile_signals->add_security_event_providers("provider9");

  // DISCLAIMER: If you are updating these expectations it is because you have
  // modified the version of our report. Any changes should be done behind a
  // flag and reflected on the server.
  EXPECT_CALL(*client_,
              GenerateAttestationBlob(
                  "flow_name", "0bT913JDMJpfT3oL0NoRukhI5H8e9ybnyPiXgBD+bJs=",
                  "shloxL+uLwC7hyH5SxqtLSP0JylONKpUn8LJWo56XeU=",
                  "JZwzQsABfEiksUVXG+BiJ5Cmqa2YxeY9utdCViMtSMo=", testing::_))
      .WillOnce(base::test::RunOnceCallback<4>(BlobGenerationResult{
          .attestation_blob = "fake_blob", .error_message = ""}));

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload_2",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  EXPECT_EQ(test_future.Get().blob_generation_result.attestation_blob,
            "fake_blob");
  EXPECT_EQ(test_future.Get().content_binding_version, 1);
}

// This test validates the hashes that will be used for the content binding
// on Android when using a specific report.
TEST_F(DeviceAttestationServiceAndroidTest,
       GetAttestationResponse_WithoutContentBindingVersioning) {
  // DISCLAIMER: If you are updating this test it is because you have modified
  // the version of our report. Any changes should be done behind a flag and
  // reflected on the server.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      enterprise_signals::features::kContentBindingVersioningEnabled);

  enterprise_management::ChromeProfileReportRequest report;
  // DISCLAIMER: If you are updating these expectations it is because you have
  // modified the version of our report. Any changes should be done behind a
  // flag and reflected on the server.
  EXPECT_CALL(*client_,
              GenerateAttestationBlob(
                  "flow_name", "+cn6FsQp0HHW5cZrvbmJbLqSsx16EbZakvOiu1p4k6k=",
                  "shloxL+uLwC7hyH5SxqtLSP0JylONKpUn8LJWo56XeU=",
                  "JZwzQsABfEiksUVXG+BiJ5Cmqa2YxeY9utdCViMtSMo=", testing::_))
      .WillOnce(base::test::RunOnceCallback<4>(BlobGenerationResult{
          .attestation_blob = "fake_blob", .error_message = ""}));

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload_2",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  EXPECT_EQ(test_future.Get().blob_generation_result.attestation_blob,
            "fake_blob");
  EXPECT_EQ(test_future.Get().content_binding_version, 0);
}

}  // namespace enterprise
