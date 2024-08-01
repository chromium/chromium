// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "base/strings/string_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user_manager.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "dbus/object_path.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

constexpr char kProfilePath[] = "/network/profile/0";
constexpr char kVpnServiceName[] = "vpn_service_name";
constexpr char kVpnHostname[] = "vpn_host_name";
constexpr char kOpenVpnUsername[] = "username";
constexpr char kOpenVpnPassword[] = "password123";

void CheckPropertyStringValue(const base::Value::Dict* dict,
                              const std::string& key,
                              const std::string& expected) {
  const std::string* actual = dict->FindString(key);
  ASSERT_TRUE(actual);
  EXPECT_EQ(*actual, expected);
}

class VpnUiInteractiveUiTest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();

    ShillServiceClient::Get()->GetTestInterface()->ClearServices();
    ShillProfileClient::Get()->GetTestInterface()->AddProfile(
        kProfilePath,
        user_manager::UserManager::Get()->GetActiveUser()->username_hash());
    ShillProfileClient::Get()->SetProperty(
        dbus::ObjectPath(kProfilePath), shill::kAlwaysOnVpnModeProperty,
        base::Value(shill::kAlwaysOnVpnModeOff), base::DoNothing(),
        base::DoNothing());

    // Load a fake cert database to user NSSDB. VPN network has to be connected
    // after the NetworkCertLoader finishes loading initial certs.
    test_nsscertdb_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
    NetworkCertLoader::ForceAvailableForNetworkAuthForTesting();
    NetworkCertLoader::Get()->SetUserNSSDB(test_nsscertdb_.get());
  }

 private:
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;
};

IN_PROC_BROWSER_TEST_F(VpnUiInteractiveUiTest,
                       BuiltInOpenVpnAddConnectDisconnectForget) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Open add built-in VPN dialog"),

      OpenAddBuiltInVpnDialog(kOSSettingsId),

      Log("Verify expected dialog title shows"),

      WaitForElementTextContains(
          kOSSettingsId, settings::InternetConfigDialogTitle(),
          l10n_util::GetStringFUTF8(
              IDS_SETTINGS_INTERNET_JOIN_TYPE,
              l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_VPN))),

      Log("Verify the connect button is disabled before input the config"),

      WaitForElementDisabled(kOSSettingsId,
                             settings::vpn::JoinVpnDialogConnectButton()),

      Log("Enter VPN service name"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::vpn::JoinVpnDialogServiceNameInput()),
      ClickElement(kOSSettingsId,
                   settings::vpn::JoinVpnDialogServiceNameInput()),
      SendTextAsKeyEvents(kOSSettingsId, kVpnServiceName),

      Log("Enter VPN hostname"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::vpn::JoinVpnDialogHostnameInput()),
      ClickElement(kOSSettingsId, settings::vpn::JoinVpnDialogHostnameInput()),
      SendTextAsKeyEvents(kOSSettingsId, kVpnHostname),

      Log("Choose VPN provider type"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::vpn::JoinVpnDialogProviderTypeSelect()),
      ClickElement(kOSSettingsId,
                   settings::vpn::JoinVpnDialogProviderTypeSelect()),
      SelectDropdownElementOption(
          kOSSettingsId, settings::vpn::JoinVpnDialogProviderTypeOptions(),
          l10n_util::GetStringUTF8(IDS_ONC_VPN_TYPE_OPENVPN).c_str()),
      ClickElement(kOSSettingsId, settings::InternetConfigDialogTitle()),

      Log("Enter VPN username"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::vpn::JoinVpnDialogOpenVpnUsernameInput()),
      MoveMouseTo(kOSSettingsId,
                  settings::vpn::JoinVpnDialogOpenVpnUsernameInput()),
      ClickElement(kOSSettingsId,
                   settings::vpn::JoinVpnDialogOpenVpnUsernameInput()),
      SendTextAsKeyEvents(kOSSettingsId, kOpenVpnUsername),

      Log("Enter VPN password"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::vpn::JoinVpnDialogOpenVpnPasswordInput()),
      ClickElement(kOSSettingsId,
                   settings::vpn::JoinVpnDialogOpenVpnPasswordInput()),
      SendTextAsKeyEvents(kOSSettingsId, kOpenVpnPassword),

      Log("Checking for OTP and save credentials element shows"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::vpn::JoinVpnDialogOpenVpnOtpInput()),
      WaitForElementEnabled(
          kOSSettingsId, settings::vpn::JoinVpnDialogSaveCredentialsToggle()),

      Log("Click on \"Connect\" button"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::vpn::JoinVpnDialogConnectButton()),
      ClickElement(kOSSettingsId, settings::vpn::JoinVpnDialogConnectButton()),

      Log("Verify VPN summary item has expected label"),

      WaitForElementEnabled(kOSSettingsId, settings::vpn::VpnSummaryItem()),
      WaitForElementTextContains(kOSSettingsId, settings::vpn::VpnSummaryItem(),
                                 kVpnServiceName),

      Log("Verify the expected VPN service is created in shill"),

      Do([&]() {
        const std::string vpn_service_path =
            ShillServiceClient::Get()
                ->GetTestInterface()
                ->FindServiceMatchingName(kVpnServiceName);
        ASSERT_FALSE(vpn_service_path.empty());
        const base::Value::Dict* properties =
            ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
                vpn_service_path);
        ASSERT_TRUE(properties);
        const base::Value::Dict* provider =
            properties->FindDict(shill::kProviderProperty);
        ASSERT_TRUE(provider);
        CheckPropertyStringValue(provider, shill::kHostProperty, kVpnHostname);
        CheckPropertyStringValue(provider, shill::kOpenVPNUserProperty,
                                 kOpenVpnUsername);
        CheckPropertyStringValue(provider, shill::kOpenVPNPasswordProperty,
                                 kOpenVpnPassword);
        CheckPropertyStringValue(provider, shill::kTypeProperty,
                                 shill::kProviderOpenVpn);
      }),

      Log("Navigate to VPN detail subpage"),

      NavigateToInternetDetailsPage(kOSSettingsId, NetworkTypePattern::VPN(),
                                    kVpnServiceName),
      WaitForElementTextContains(
          kOSSettingsId, settings::SettingsSubpageNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),

      Log("Checking for the expected UI elements"),

      WaitForElementTextContains(
          kOSSettingsId, ash::settings::vpn::VpnSubpageProviderType(),
          l10n_util::GetStringUTF8(IDS_ONC_VPN_TYPE_OPENVPN).c_str()),
      WaitForElementExists(kOSSettingsId,
                           ash::settings::vpn::VpnSubpageHostnameInput()),

      CheckJsResultAt(
          kOSSettingsId, ash::settings::vpn::VpnSubpageHostnameInput(),
          base::StringPrintf("(el) => el.value === '%s'", kVpnHostname)),
      WaitForElementExists(kOSSettingsId,
                           ash::settings::vpn::VpnSubpageUsernameInput()),
      CheckJsResultAt(
          kOSSettingsId, ash::settings::vpn::VpnSubpageUsernameInput(),
          base::StringPrintf("(el) => el.value === '%s'", kOpenVpnUsername)),

      Log("Disconnect VPN network"),

      WaitForElementEnabled(
          kOSSettingsId,
          ash::settings::SettingsSubpageConnectDisconnectButton()),
      ClickElement(kOSSettingsId,
                   ash::settings::SettingsSubpageConnectDisconnectButton()),
      WaitForElementTextContains(
          kOSSettingsId, settings::SettingsSubpageNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_NOT_CONNECTED).c_str()),

      Log("Forget VPN network"),

      WaitForElementEnabled(kOSSettingsId,
                            ash::settings::SettingsSubpageForgetButton()),
      ClickElement(kOSSettingsId, ash::settings::SettingsSubpageForgetButton()),

      Log("Verify the VPN doesn't appear in the list after forget"),

      WaitForElementDisplayNone(kOSSettingsId,
                                ash::settings::vpn::VpnNetworksList()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
