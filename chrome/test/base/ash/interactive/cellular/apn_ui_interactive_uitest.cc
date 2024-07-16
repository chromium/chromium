// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/fake_hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "dbus/object_path.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

const char kNewApnName[] = "newApnName";

class ApnUiInteractiveUiTest : public EsimInteractiveUiTestBase {
 protected:
  ApnUiInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kApnRevamp);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest,
                       NonConnectedCellularHasNoApn) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Navigate to the APN revamp details page"),

      NavigateToApnRevampDetailsPage(kOSSettingsId),

      Log("Verify it connects to the auto detected APN"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::ApnListFirstItemName(),
          /*text=*/FakeHermesEuiccClient::kFakeDefaultApn),

      Log("Disconnect cellular network"),

      Do([&]() { DisconnectEsimService(); }),

      Log("Verify Zero state message shows and no APN shows in the list when "
          "not connected"),

      WaitForElementExists(kOSSettingsId,
                           settings::cellular::ApnSubpageZeroStateContent()),
      WaitForElementHasAttribute(
          kOSSettingsId, settings::cellular::ApnListFirstItem(), "hidden"),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest, CreateDefaultCustomApn) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Verify no custom APNs before start testing"),

      Do([&]() {
        const base::Value::Dict* cellular_properties =
            ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
                esim_info().service_path());
        ASSERT_TRUE(cellular_properties);
        const base::Value::List* shill_custom_apns =
            cellular_properties->FindList(
                shill::kCellularCustomApnListProperty);
        ASSERT_TRUE(shill_custom_apns);
        EXPECT_EQ(0u, shill_custom_apns->size());
      }),

      Log("Navigating to the internet page"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Navigate to the APN revamp details page"),

      NavigateToApnRevampDetailsPage(kOSSettingsId),

      Log("Open add custom APN dialog"),

      OpenAddCustomApnDetailsDialog(kOSSettingsId),

      Log("Type in custom APN name in APN dialog"),

      WaitForElementExists(kOSSettingsId,
                           settings::cellular::ApnDialogApnInput()),
      WaitForElementEnabled(kOSSettingsId,
                            settings::cellular::ApnDialogApnInput()),
      ClickElement(kOSSettingsId, settings::cellular::ApnDialogApnInput()),
      SendTextAsKeyEvents(kOSSettingsId, kNewApnName),

      Log("Check APN type defaults to \'Default\'"),

      WaitForElementEnabled(
          kOSSettingsId, settings::cellular::ApnDialogAdvancedSettingsButton()),
      ClickElement(kOSSettingsId,
                   settings::cellular::ApnDialogAdvancedSettingsButton()),
      WaitForElementChecked(kOSSettingsId,
                            settings::cellular::ApnDialogDefaultCheckbox()),

      Log("Save the custom APN"),

      WaitForElementExists(kOSSettingsId,
                           settings::cellular::ApnDialogAddActionButton()),
      WaitForElementEnabled(kOSSettingsId,
                            settings::cellular::ApnDialogAddActionButton()),
      ClickElement(kOSSettingsId,
                   settings::cellular::ApnDialogAddActionButton()),

      Log("Wait for the newly created custom APN appear at the top of the "
          "list"),

      WaitForElementExists(kOSSettingsId,
                           settings::cellular::ApnListFirstItem()),
      WaitForElementTextContains(kOSSettingsId,
                                 settings::cellular::ApnListFirstItemName(),
                                 /*text=*/kNewApnName),

      Log("Verify the custom APN saved in Shill"),

      Do([&]() {
        const base::Value::Dict* cellular_properties =
            ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
                esim_info().service_path());
        ASSERT_TRUE(cellular_properties);
        const base::Value::List* shill_custom_apns =
            cellular_properties->FindList(
                shill::kCellularCustomApnListProperty);
        ASSERT_TRUE(shill_custom_apns);
        EXPECT_EQ(1u, shill_custom_apns->size());
        const std::string* apn_name =
            shill_custom_apns->front().GetDict().FindString(
                shill::kApnProperty);
        EXPECT_EQ(kNewApnName, *apn_name);
        const std::string* apn_type =
            shill_custom_apns->front().GetDict().FindString(
                shill::kApnTypesProperty);
        EXPECT_EQ(shill::kApnTypeDefault, *apn_type);
      }),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
