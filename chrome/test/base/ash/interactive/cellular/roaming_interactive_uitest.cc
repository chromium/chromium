// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/components/onc/onc_utils.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

class RoamingInteractiveUITest : public EsimInteractiveUiTestBase {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    esim_info_ = std::make_unique<SimInfo>(/*id=*/0);
    ConfigureEsimProfile(euicc_info(), *esim_info_, /*connected=*/true);
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  RestrictCellularRoaming() {
    return Steps(Do([this]() {
      base::Value::Dict global_config;

      auto onc_configs = base::Value::List();
      onc_configs.Append(GenerateCellularPolicy(*esim_info_,
                                                /*allow_apn_modificaiton=*/true,
                                                /*allow_roaming=*/false));

      NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
          ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
          std::move(onc_configs), global_config);
    }));
  }

  const SimInfo& esim_info() const { return *esim_info_; }

 private:
  std::string device_path_;
  std::unique_ptr<SimInfo> esim_info_;
};

IN_PROC_BROWSER_TEST_F(RoamingInteractiveUITest, RestrictRoamingWithPolicy) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      WaitForElementDoesNotExist(
          kOSSettingsId, settings::cellular::
                             CellularDetailsAllowDataRoamingTogglePolicyIcon()),

      Log("Prohibiting roaming with policy"),

      RestrictCellularRoaming(),

      Log("Checking that roaming toggle has an enterprise icon"),

      WaitForElementExists(
          kOSSettingsId, settings::cellular::
                             CellularDetailsAllowDataRoamingTogglePolicyIcon()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
