// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

class EsimUiElementsInteractiveUiTest : public EsimInteractiveUiTestBase {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    esim_info_ = std::make_unique<SimInfo>(/*id=*/0);
    ConfigureEsimProfile(euicc_info(), *esim_info_, /*connected=*/true);
  }

  const SimInfo& esim_info() const { return *esim_info_; }

 private:
  std::unique_ptr<SimInfo> esim_info_;
};

IN_PROC_BROWSER_TEST_F(EsimUiElementsInteractiveUiTest, OsSettingsDetailsPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the details page for the connected eSIM network"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::InternetSettingsSubpageTitle(),
                                 /*text=*/esim_info().nickname()),

      Log("Checking for the expected UI elements"),

      WaitForElementTextContains(
          kOSSettingsId, settings::SettingsSubpageNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularDetailsNetworkOperator()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularDetailsSubpageAutoConnectToggle()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularDetailsAllowDataRoamingToggle()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularDetailsAdvancedSection()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularDetailsConfigurableSection()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularDetailsProxySection()),

      Log("Disconnecting the eSIM network"),

      Do([this]() { esim_info().Disconnect(); }),

      Log("Checking that the UI shows the network is disconnected"),

      WaitForElementTextContains(
          kOSSettingsId, settings::SettingsSubpageNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_NOT_CONNECTED).c_str()),

      Log("Disabling the eSIM network"),

      Do([this]() {
        auto* hermes_profile_client = HermesProfileClient::Get();
        CHECK(hermes_profile_client);
        hermes_profile_client->DisableCarrierProfile(
            dbus::ObjectPath(esim_info().profile_path()), base::DoNothing());
      }),

      Log("Checking that the network operator is not shown"),

      WaitForElementDoesNotExist(
          kOSSettingsId,
          ash::settings::cellular::CellularDetailsNetworkOperator()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
