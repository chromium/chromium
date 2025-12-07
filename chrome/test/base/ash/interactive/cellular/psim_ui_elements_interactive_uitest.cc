// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "dbus/object_path.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

class PsimUiElementsUiTest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();

    psim_info_ = std::make_unique<SimInfo>(/*id=*/0);
    helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    helper_->device_test()->AddDevice(euicc_info_.path(), shill::kTypeCellular,
                                      psim_info_->name());

    helper_->service_test()->AddService(
        psim_info_->service_path(), psim_info_->guid(), psim_info_->name(),
        shill::kTypeCellular, shill::kStateOnline,
        /*visible=*/true);
  }

  void TearDownOnMainThread() override { helper_.reset(); }

  const SimInfo& psim_info() const { return *psim_info_; }

 private:
  const EuiccInfo euicc_info_{/*id=*/0};
  std::unique_ptr<SimInfo> psim_info_;
  std::unique_ptr<NetworkStateTestHelper> helper_;
};

IN_PROC_BROWSER_TEST_F(PsimUiElementsUiTest, OsSettingsDetailsPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the details page for the pSIM network"),

      NavigateToInternetDetailsPage(
          kOSSettingsId, NetworkTypePattern::Cellular(), psim_info().name()),

      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpagePsimListTitle()),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::InternetSettingsSubpageTitle(),
                                 /*text=*/psim_info().name()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
