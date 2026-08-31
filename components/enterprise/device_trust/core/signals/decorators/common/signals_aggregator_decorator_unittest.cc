// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_trust/core/signals/decorators/common/signals_aggregator_decorator.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/device_signals/core/browser/mock_signals_aggregator.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::UnorderedElementsAre;

namespace enterprise_connectors {

namespace {

constexpr char kOs[] = "iOS";
constexpr char kOsVersion[] = "17.4";
constexpr char kBrowserVersion[] = "120.0";
constexpr char kDeviceModel[] = "iPhone 15";
constexpr char kDeviceManufacturer[] = "Apple";
constexpr char kDisplayName[] = "User iPhone";
constexpr char kHostName[] = "iPhone-Host";
constexpr char kVendorId[] = "vendor-123";
constexpr char kSerialNumber[] = "SN-12345";
constexpr char kDeviceEnrollmentDomain[] = "example.com";
constexpr char kWindowsMachineDomain[] = "win-machine.com";
constexpr char kWindowsUserDomain[] = "win-user.com";
constexpr char kUserEnrollmentDomain[] = "example.com";
constexpr char kMacAddress[] = "00:00:00:00:00:00";
constexpr char kDnsServer[] = "8.8.8.8";
constexpr char kDeviceAffiliationId[] = "device-affiliation-id";
constexpr char kProfileAffiliationId[] = "profile-affiliation-id";

device_signals::SignalsAggregationResponse CreateSuccessResponse() {
  device_signals::SignalsAggregationResponse response{};

  device_signals::OsSignalsResponse os_signals{};
  os_signals.collection_error = std::nullopt;
  os_signals.os_version = kOsVersion;
  os_signals.operating_system = kOs;
  os_signals.browser_version = kBrowserVersion;
  os_signals.device_model = kDeviceModel;
  os_signals.device_manufacturer = kDeviceManufacturer;
  os_signals.display_name = kDisplayName;
  os_signals.hostname = kHostName;
  os_signals.vendor_id = kVendorId;
  os_signals.serial_number = kSerialNumber;
  os_signals.device_enrollment_domain = kDeviceEnrollmentDomain;
  os_signals.windows_machine_domain = kWindowsMachineDomain;
  os_signals.windows_user_domain = kWindowsUserDomain;
  os_signals.disk_encryption = device_signals::SettingValue::ENABLED;
  os_signals.screen_lock_secured = device_signals::SettingValue::ENABLED;
  os_signals.os_firewall = device_signals::SettingValue::ENABLED;
  os_signals.secure_boot_mode = device_signals::SettingValue::ENABLED;
  os_signals.mac_addresses = std::vector<std::string>{kMacAddress};
  os_signals.system_dns_servers = std::vector<std::string>{kDnsServer};
  os_signals.device_affiliation_ids =
      std::vector<std::string>{kDeviceAffiliationId};
  response.os_signals_response = std::move(os_signals);

  device_signals::ProfileSignalsResponse profile_signals{};
  profile_signals.collection_error = std::nullopt;
  profile_signals.built_in_dns_client_enabled = true;
  profile_signals.chrome_remote_desktop_app_blocked = true;
  profile_signals.safe_browsing_protection_level =
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
  profile_signals.site_isolation_enabled = true;
  profile_signals.realtime_url_check_mode =
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED;
  profile_signals.password_protection_warning_trigger =
      safe_browsing::PASSWORD_REUSE;
  profile_signals.profile_enrollment_domain = kUserEnrollmentDomain;
  profile_signals.profile_affiliation_ids =
      std::vector<std::string>{kProfileAffiliationId};
  response.profile_signals_response = std::move(profile_signals);

  return response;
}

void CheckSignalValue(const base::DictValue& dict,
                      std::string_view signal_name,
                      const char* expected_value) {
  const std::string* signal_value = dict.FindString(signal_name);
  ASSERT_NE(signal_value, nullptr);
  EXPECT_EQ(*signal_value, expected_value);
}

void CheckSignalValue(const base::DictValue& dict,
                      std::string_view signal_name,
                      int expected_value) {
  std::optional<int> value = dict.FindInt(signal_name);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), expected_value);
}

void CheckSignalValue(const base::DictValue& dict,
                      std::string_view signal_name,
                      bool expected_value) {
  std::optional<bool> value = dict.FindBool(signal_name);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), expected_value);
}

}  // namespace

class SignalsAggregatorDecoratorTest : public testing::Test {
 protected:
  SignalsAggregatorDecoratorTest() {
    mock_signals_aggregator_ = std::make_unique<
        testing::StrictMock<device_signals::MockSignalsAggregator>>();
    decorator_ = std::make_unique<SignalsAggregatorDecorator>(
        mock_signals_aggregator_.get());
  }

  base::DictValue DecorateSignals(
      device_signals::SignalsAggregationResponse response,
      base::DictValue initial_dict = base::DictValue()) {
    EXPECT_CALL(
        *mock_signals_aggregator_,
        GetSignals(
            AllOf(
                Field(&device_signals::SignalsAggregationRequest::signal_names,
                      UnorderedElementsAre(
                          device_signals::SignalName::kOsSignals,
                          device_signals::SignalName::kBrowserContextSignals))),
            _))
        .WillOnce(base::test::RunOnceCallback<1>(std::move(response)));

    base::DictValue dict = std::move(initial_dict);
    base::RunLoop run_loop;
    decorator_->Decorate(dict, run_loop.QuitClosure());
    run_loop.Run();
    return dict;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<device_signals::MockSignalsAggregator>
      mock_signals_aggregator_;
  std::unique_ptr<SignalsAggregatorDecorator> decorator_;
};

// Verifies that a fully successful bundle from the SignalsAggregator decorates
// all keys into the dictionary.
TEST_F(SignalsAggregatorDecoratorTest, DecorateSuccess) {
  base::DictValue actual_dict = DecorateSignals(CreateSuccessResponse());

  // Strings
  CheckSignalValue(actual_dict, device_signals::names::kOs, kOs);
  CheckSignalValue(actual_dict, device_signals::names::kOsVersion, kOsVersion);
  CheckSignalValue(actual_dict, device_signals::names::kBrowserVersion,
                   kBrowserVersion);
  CheckSignalValue(actual_dict, device_signals::names::kDeviceModel,
                   kDeviceModel);
  CheckSignalValue(actual_dict, device_signals::names::kDeviceManufacturer,
                   kDeviceManufacturer);
  CheckSignalValue(actual_dict, device_signals::names::kDeviceHostName,
                   kHostName);
  CheckSignalValue(actual_dict, device_signals::names::kDisplayName,
                   kDisplayName);
  CheckSignalValue(actual_dict, device_signals::names::kVendorId, kVendorId);
  CheckSignalValue(actual_dict, device_signals::names::kDeviceEnrollmentDomain,
                   kDeviceEnrollmentDomain);
  CheckSignalValue(actual_dict, device_signals::names::kSerialNumber,
                   kSerialNumber);
  CheckSignalValue(actual_dict, device_signals::names::kWindowsMachineDomain,
                   kWindowsMachineDomain);
  CheckSignalValue(actual_dict, device_signals::names::kWindowsUserDomain,
                   kWindowsUserDomain);
  CheckSignalValue(actual_dict, device_signals::names::kUserEnrollmentDomain,
                   kUserEnrollmentDomain);

  // Enums & Ints
  CheckSignalValue(actual_dict, device_signals::names::kDiskEncrypted,
                   static_cast<int>(device_signals::SettingValue::ENABLED));
  CheckSignalValue(actual_dict, device_signals::names::kScreenLockSecured,
                   static_cast<int>(device_signals::SettingValue::ENABLED));
  CheckSignalValue(actual_dict, device_signals::names::kOsFirewall,
                   static_cast<int>(device_signals::SettingValue::ENABLED));
  CheckSignalValue(actual_dict, device_signals::names::kSecureBootEnabled,
                   static_cast<int>(device_signals::SettingValue::ENABLED));
  CheckSignalValue(
      actual_dict, device_signals::names::kSafeBrowsingProtectionLevel,
      static_cast<int>(safe_browsing::SafeBrowsingState::STANDARD_PROTECTION));
  CheckSignalValue(
      actual_dict, device_signals::names::kRealtimeUrlCheckMode,
      static_cast<int>(
          enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED));
  CheckSignalValue(
      actual_dict, device_signals::names::kPasswordProtectionWarningTrigger,
      static_cast<int>(DeviceTrustPasswordProtectionTrigger::kPasswordReuse));
  CheckSignalValue(
      actual_dict, device_signals::names::kTrigger,
      static_cast<int>(device_signals::Trigger::kBrowserNavigation));

  // Booleans
  CheckSignalValue(actual_dict, device_signals::names::kBuiltInDnsClientEnabled,
                   true);
  CheckSignalValue(actual_dict,
                   device_signals::names::kChromeRemoteDesktopAppBlocked, true);
  CheckSignalValue(actual_dict, device_signals::names::kSiteIsolationEnabled,
                   true);

  // Lists
  const base::ListValue* mac_addresses =
      actual_dict.FindList(device_signals::names::kMacAddresses);
  ASSERT_NE(mac_addresses, nullptr);
  ASSERT_EQ(mac_addresses->size(), 1u);
  EXPECT_EQ(mac_addresses->front().GetString(), kMacAddress);

  const base::ListValue* dns_servers =
      actual_dict.FindList(device_signals::names::kSystemDnsServers);
  ASSERT_NE(dns_servers, nullptr);
  ASSERT_EQ(dns_servers->size(), 1u);
  EXPECT_EQ(dns_servers->front().GetString(), kDnsServer);

  const base::ListValue* device_affiliation_ids =
      actual_dict.FindList(device_signals::names::kDeviceAffiliationIds);
  ASSERT_NE(device_affiliation_ids, nullptr);
  ASSERT_EQ(device_affiliation_ids->size(), 1u);
  EXPECT_EQ(device_affiliation_ids->front().GetString(), kDeviceAffiliationId);

  const base::ListValue* profile_affiliation_ids =
      actual_dict.FindList(device_signals::names::kProfileAffiliationIds);
  ASSERT_NE(profile_affiliation_ids, nullptr);
  ASSERT_EQ(profile_affiliation_ids->size(), 1u);
  EXPECT_EQ(profile_affiliation_ids->front().GetString(),
            kProfileAffiliationId);
}

// Verifies that if the OS bundle encounters a collection error,
// it is skipped without destroying the entire payload.
TEST_F(SignalsAggregatorDecoratorTest, DecorateIgnoresErrorBundles) {
  device_signals::SignalsAggregationResponse response = CreateSuccessResponse();
  response.os_signals_response->collection_error =
      device_signals::SignalCollectionError::kUnsupported;

  base::DictValue actual_dict = DecorateSignals(std::move(response));

  // OS signals should be missing because the bundle failed.
  EXPECT_EQ(actual_dict.FindString(device_signals::names::kOs), nullptr);

  // Profile signals should still be present.
  CheckSignalValue(actual_dict, device_signals::names::kBuiltInDnsClientEnabled,
                   true);
  CheckSignalValue(
      actual_dict, device_signals::names::kPasswordProtectionWarningTrigger,
      static_cast<int>(DeviceTrustPasswordProtectionTrigger::kPasswordReuse));
}

// Verifies that if the profile bundle encounters a collection error,
// it is skipped without destroying the OS signals.
TEST_F(SignalsAggregatorDecoratorTest, DecorateIgnoresProfileErrorBundles) {
  device_signals::SignalsAggregationResponse response = CreateSuccessResponse();
  response.profile_signals_response->collection_error =
      device_signals::SignalCollectionError::kUnsupported;

  base::DictValue actual_dict = DecorateSignals(std::move(response));

  // OS signals should still be present.
  CheckSignalValue(actual_dict, device_signals::names::kOs, kOs);

  // Profile signals should be missing because the bundle failed.
  EXPECT_FALSE(
      actual_dict.contains(device_signals::names::kBuiltInDnsClientEnabled));
  EXPECT_EQ(
      actual_dict.FindString(device_signals::names::kUserEnrollmentDomain),
      nullptr);
}

// Verifies that a global top-level error preserves signals added by previous
// decorators and does not add aggregator-backed signals.
TEST_F(SignalsAggregatorDecoratorTest, DecorateTopLevelError) {
  device_signals::SignalsAggregationResponse response = CreateSuccessResponse();
  response.top_level_error =
      device_signals::SignalCollectionError::kUnsupported;

  base::DictValue initial_dict;
  initial_dict.Set(device_signals::names::kDisplayName, "existing");

  base::DictValue actual_dict =
      DecorateSignals(std::move(response), std::move(initial_dict));

  CheckSignalValue(actual_dict, device_signals::names::kDisplayName,
                   "existing");
  EXPECT_FALSE(actual_dict.contains(device_signals::names::kOs));
  EXPECT_FALSE(
      actual_dict.contains(device_signals::names::kBuiltInDnsClientEnabled));
  EXPECT_FALSE(actual_dict.contains(device_signals::names::kTrigger));
  EXPECT_EQ(actual_dict.size(), 1u);
}

// Verifies that a missing password protection trigger policy maps to 0
// (kUnset).
TEST_F(SignalsAggregatorDecoratorTest, DecorateUnsetPasswordTrigger) {
  device_signals::SignalsAggregationResponse response = CreateSuccessResponse();
  response.profile_signals_response->password_protection_warning_trigger =
      std::nullopt;

  base::DictValue actual_dict = DecorateSignals(std::move(response));

  CheckSignalValue(
      actual_dict, device_signals::names::kPasswordProtectionWarningTrigger,
      static_cast<int>(DeviceTrustPasswordProtectionTrigger::kUnset));
}

// Verifies that empty strings for optional fields are dropped entirely from the
// dictionary, while required schema fields (like display_name, mac_addresses,
// and system_dns_servers) are retained as empty string/lists.
TEST_F(SignalsAggregatorDecoratorTest, DecorateEmptyAndMissingValues) {
  device_signals::SignalsAggregationResponse response = CreateSuccessResponse();
  response.os_signals_response->vendor_id = "";
  response.os_signals_response->display_name = std::nullopt;
  response.os_signals_response->mac_addresses = std::nullopt;
  response.os_signals_response->system_dns_servers = std::nullopt;
  response.os_signals_response->device_affiliation_ids = {};
  response.profile_signals_response->profile_affiliation_ids = {};

  base::DictValue actual_dict = DecorateSignals(std::move(response));

  EXPECT_EQ(actual_dict.FindString(device_signals::names::kVendorId), nullptr);
  CheckSignalValue(actual_dict, device_signals::names::kDisplayName, "");

  const base::ListValue* mac_addresses =
      actual_dict.FindList(device_signals::names::kMacAddresses);
  ASSERT_NE(mac_addresses, nullptr);
  EXPECT_TRUE(mac_addresses->empty());

  const base::ListValue* dns_servers =
      actual_dict.FindList(device_signals::names::kSystemDnsServers);
  ASSERT_NE(dns_servers, nullptr);
  EXPECT_TRUE(dns_servers->empty());

  const base::ListValue* device_affiliation_ids =
      actual_dict.FindList(device_signals::names::kDeviceAffiliationIds);
  ASSERT_NE(device_affiliation_ids, nullptr);
  EXPECT_TRUE(device_affiliation_ids->empty());

  const base::ListValue* profile_affiliation_ids =
      actual_dict.FindList(device_signals::names::kProfileAffiliationIds);
  ASSERT_NE(profile_affiliation_ids, nullptr);
  EXPECT_TRUE(profile_affiliation_ids->empty());
}

}  // namespace enterprise_connectors
