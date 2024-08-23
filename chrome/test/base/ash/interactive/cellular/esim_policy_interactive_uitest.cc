// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

const char kCellularPolicyPattern[] =
    R"({
      "GUID": "%s",
      "Type": "Cellular",
      "Name": "cellular_policy",
      "Cellular": {
        "SMDPAddress": "LPA:1$SMDP.GSMA.COM$123",
        "ICCID": "%s"
      }
    })";

class EsimPolicyInteractiveUiTest : public EsimInteractiveUiTestBase {
 public:
  std::optional<bool> NetworkIsConnected(const std::string& service_path) {
    const NetworkState* network =
        NetworkHandler::Get()->network_state_handler()->GetNetworkState(
            service_path);
    if (!network) {
      return std::nullopt;
    }
    return network->IsConnectedState();
  }

 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    esim_info_ = std::make_unique<SimInfo>(/*id=*/0);
    ConfigureEsimProfile(euicc_info(), *esim_info_, /*connected=*/true);
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  RestrictCellularFromPolicy(bool allow_only_managed_cellular,
                             bool set_existing_esim_as_managed) {
    return Steps(Do([allow_only_managed_cellular, set_existing_esim_as_managed,
                     this]() {
      base::Value::Dict global_config;
      global_config.Set(
          ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks,
          allow_only_managed_cellular);
      std::optional<base::Value::Dict> cellular_config =
          chromeos::onc::ReadDictionaryFromJson(base::StringPrintf(
              kCellularPolicyPattern, esim_info_->guid().c_str(),
              esim_info_->iccid().c_str()));
      ASSERT_TRUE(cellular_config.has_value());

      auto onc_configs = base::Value::List();
      if (set_existing_esim_as_managed) {
        onc_configs.Append(cellular_config->Clone());
      }

      NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
          ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
          std::move(onc_configs), global_config);
    }));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  ClickEsimProfileInMobilePage(const ui::ElementIdentifier& element_id,
                               const std::string& esim_profile_name) {
    return Steps(NavigateSettingsToNetworkSubpage(
                     element_id, ash::NetworkTypePattern::Mobile()),
                 ClickAnyElementTextContains(
                     element_id, settings::cellular::CellularNetworksList(),
                     WebContentsInteractionTestUtil::DeepQuery({
                         "network-list",
                         "network-list-item",
                         "div#divText",
                     }),
                     esim_profile_name));
  }

  const SimInfo& esim_info() const { return *esim_info_; }

 private:
  std::unique_ptr<SimInfo> esim_info_;
};

IN_PROC_BROWSER_TEST_F(EsimPolicyInteractiveUiTest,
                       EsimPolicyRestrictUnmanagedNetworkConnection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingStateObserver<std::optional<bool>>,
      kNetworkConnectedState);

  WebContentsInteractionTestUtil::DeepQuery cellular_network_list_item_label(
      {"network-list", "network-list-item", "div#divText"});

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      NavigateSettingsToNetworkSubpage(kOSSettingsId,
                                       ash::NetworkTypePattern::Mobile()),
      PollState(kNetworkConnectedState,
                base::BindRepeating(
                    &EsimPolicyInteractiveUiTest::NetworkIsConnected,
                    base::Unretained(this), esim_info().service_path())),
      WaitForState(kNetworkConnectedState, true),

      Log("Check for add eSIM icon is enabled"),

      WaitForElementEnabled(kOSSettingsId, settings::cellular::AddEsimButton()),

      Log("Apply policy to restrict unmanaged cellular network connection"),

      RestrictCellularFromPolicy(/*allow_only_managed_cellular=*/true,
                                 /*set_existing_esim_as_managed=*/false),

      Log("Check for add eSIM icon disabled"),

      WaitForElementDisabled(kOSSettingsId,
                             settings::cellular::AddEsimButton()),

      Log("Check for unmanaged cellular network should be disconnected by "
          "policy"),

      WaitForState(kNetworkConnectedState, false),

      Log("Click on the network item should go to its detail page"),

      ClickAnyElementTextContains(
          kOSSettingsId, settings::cellular::CellularNetworksList(),
          cellular_network_list_item_label, esim_info().nickname()),
      WaitForElementTextContains(kOSSettingsId,
                                 settings::InternetSettingsSubpageTitle(),
                                 /*text=*/esim_info().nickname()),

      Log("Check the connect button is disabled"),

      WaitForElementDisabled(
          kOSSettingsId, settings::SettingsSubpageConnectDisconnectButton()),

      Log("Check for more action button should be enabled on unmanaged "
          "network"),

      WaitForElementEnabled(kOSSettingsId,
                            settings::NetworkMoreDetailsMenuButton()),

      Log("Apply policy to set the existing cellular network as managed one"),

      RestrictCellularFromPolicy(/*allow_only_managed_cellular=*/true,
                                 /*set_existing_esim_as_managed=*/true),

      Log("Click on managed network should trigger connection"),

      ClickEsimProfileInMobilePage(kOSSettingsId, esim_info().nickname()),
      WaitForAnyElementTextContains(
          kOSSettingsId, settings::cellular::CellularNetworksList(),
          WebContentsInteractionTestUtil::DeepQuery(
              {"network-list", "network-list-item", "div#sublabel"}),
          l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),
      WaitForState(kNetworkConnectedState, true),

      Log("Check for policy icon"),

      WaitForElementExists(
          kOSSettingsId,
          settings::cellular::CellularNetworkListItemPolicyIcon()),

      Log("Go to the network detailed page"),

      ClickAnyElementTextContains(
          kOSSettingsId, settings::cellular::CellularNetworksList(),
          cellular_network_list_item_label, esim_info().nickname()),

      Log("Check for policy icon"),

      WaitForElementExists(kOSSettingsId,
                           settings::SettingsSubpagePolicyIcon()),

      Log("Check for more action button should be disabled on managed network"),

      WaitForElementDisabled(kOSSettingsId,
                             settings::NetworkMoreDetailsMenuButton()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
