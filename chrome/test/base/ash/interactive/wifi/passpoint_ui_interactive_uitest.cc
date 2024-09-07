// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_service.h"
#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_subscription.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

constexpr char kPasspointId[] = "fake_id";
constexpr char kPasspointFriendlyName[] = "fake_friendly_name";
constexpr char kPasspointProvisioningSource[] = "fake_provisioning_source";
constexpr char kPasspointDomain[] = "fake_domain";
constexpr base::Time kPasspointExpirationDate =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(160000));

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

class PasspointUiInteractiveUiTest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();

    if (!connectivity::FakePasspointService::IsInitialized()) {
      connectivity::FakePasspointService::Initialize();
    }

    connectivity::FakePasspointService::Get()->ClearAll();
    connectivity::FakePasspointService::Get()->AddFakePasspointSubscription(
        connectivity::FakePasspointSubscription(
            kPasspointId, kPasspointFriendlyName, kPasspointProvisioningSource,
            /*trusted_ca=*/std::nullopt,
            kPasspointExpirationDate.InMillisecondsSinceUnixEpoch(),
            std::vector<std::string>({kPasspointDomain})));

    wifi_service_info_.ConfigureService(/*connected=*/true);

    ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
        wifi_service_info_.service_path(), shill::kPasspointIDProperty,
        base::Value(kPasspointId));
  }

  const std::string wifiName() const {
    return wifi_service_info_.service_name();
  }

 private:
  const ShillServiceInfo wifi_service_info_{/*id=*/0, shill::kTypeWifi};
};

IN_PROC_BROWSER_TEST_F(PasspointUiInteractiveUiTest,
                       PasspointSubscriptionSubpageUi) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigate to for passpoint subscriptions subpage"),

      NavigateToPasspointSubscriptionSubpage(kOSSettingsId,
                                             kPasspointFriendlyName),

      Log("Checking for expiration date"),

      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::PasspointSubpageExpirationDate(),
          base::UnlocalizedTimeFormatWithPattern(kPasspointExpirationDate,
                                                 "M/d/yyyy")),

      Log("Checking for provider name"),

      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::PasspointSubpageProviderSource(),
          kPasspointProvisioningSource),

      Log("Expand domain list"),

      ClickElement(kOSSettingsId,
                   settings::wifi::PasspointSubpageDomainExpansionButton()),
      WaitForElementOpened(kOSSettingsId,
                           settings::wifi::PasspointSubpageDomainList()),

      Log("Checking for domain"),

      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::PasspointSubpageDomainListItem(),
          kPasspointDomain),

      Log("Checking for associated networks"),

      WaitForElementTextContains(
          kOSSettingsId,
          settings::wifi::PasspointSubpageAssociatedNetworksListItem(),
          wifiName()),

      Log("Click on associated network and go to its detail page"),

      ClickElement(
          kOSSettingsId,
          settings::wifi::PasspointSubpageAssociatedNetworksListItem()),
      WaitForElementTextContains(
          kOSSettingsId, settings::InternetSettingsSubpageTitle(), wifiName()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(PasspointUiInteractiveUiTest,
                       RemovePasspointFromKnownNetworkSubpage) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigate to for Known Networks subpage"),

      NavigateToKnownNetworksPage(kOSSettingsId),

      Log("Check the subscription appear in the passpoint list"),

      WaitForElementTextContains(
          kOSSettingsId,
          settings::wifi::KnownNetworksSubpagePasspointSubscriptionItem(),
          kPasspointFriendlyName),

      Log("Open drop down menu"),

      WaitForElementEnabled(
          kOSSettingsId,
          settings::wifi::KnownNetworksSubpagePasspointMoreButton()),
      ClickElement(kOSSettingsId,
                   settings::wifi::KnownNetworksSubpagePasspointMoreButton()),
      WaitForElementOpened(
          kOSSettingsId,
          settings::wifi::KnownNetworksSubpagePasspointDotsMenu()),

      Log("Click on Forget button"),

      WaitForElementExists(
          kOSSettingsId,
          settings::wifi::KnownNetworksSubpagePasspointSubscriptionForget()),
      ClickElement(
          kOSSettingsId,
          settings::wifi::KnownNetworksSubpagePasspointSubscriptionForget()),

      Log("Verify the passpoint subscription section not shown"),

      WaitForElementDisplayNone(
          kOSSettingsId,
          settings::wifi::KnownNetworksSubpagePasspointSubsciptions()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(PasspointUiInteractiveUiTest,
                       RemovePasspointFromPasspointSubpage) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigate to for passpoint subscriptions subpage"),

      NavigateToPasspointSubscriptionSubpage(kOSSettingsId,
                                             kPasspointFriendlyName),

      Log("Click on Remove button and pop up remove dialog"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::wifi::PasspointSubpageRemoveButton()),
      ClickElement(kOSSettingsId,
                   settings::wifi::PasspointSubpageRemoveButton()),
      WaitForElementExists(kOSSettingsId,
                           settings::wifi::PasspointSubpageRemoveDialog()),

      Log("Click on confirm button"),

      WaitForElementEnabled(
          kOSSettingsId,
          settings::wifi::PasspointSubpageRemoveDialogConfirmButton()),
      ClickElement(kOSSettingsId,
                   settings::wifi::PasspointSubpageRemoveDialogConfirmButton()),

      Log("Verify it goes back to Known Networks subpage and the passpoint "
          "subscription section disappear"),

      WaitForElementExists(kOSSettingsId,
                           settings::wifi::KnownNetworksSubpage()),
      WaitForElementDisplayNone(
          kOSSettingsId,
          settings::wifi::KnownNetworksSubpagePasspointSubsciptions()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
