// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/cellular/wait_for_service_connected_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                    kConnectedToFirstCellularService);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                    kConnectedToSecondCellularService);

class EsimInstallationInteractiveUiTest : public EsimInteractiveUiTestBase {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    auto* hermes_euicc_client = HermesEuiccClient::Get()->GetTestInterface();
    ASSERT_TRUE(hermes_euicc_client);

    esim_info0_ = std::make_unique<SimInfo>(/*id=*/0);

    // Add a pending profile.
    hermes_euicc_client->AddCarrierProfile(
        dbus::ObjectPath(esim_info0_->profile_path()),
        dbus::ObjectPath(euicc_info().path()), esim_info0_->iccid(),
        esim_info0_->name(), esim_info0_->nickname(),
        esim_info0_->service_provider(), esim_info0_->activation_code(),
        /*network_service_path=*/esim_info0_->service_path(),
        /*state=*/hermes::profile::State::kPending,
        /*profile_class=*/hermes::profile::ProfileClass::kOperational,
        /*add_carrier_profile_behavior=*/
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);

    esim_info1_ = std::make_unique<SimInfo>(/*id=*/1);

    // Add a pending profile.
    hermes_euicc_client->AddCarrierProfile(
        dbus::ObjectPath(esim_info1_->profile_path()),
        dbus::ObjectPath(euicc_info().path()), esim_info1_->iccid(),
        esim_info1_->name(), esim_info1_->nickname(),
        esim_info1_->service_provider(), esim_info1_->activation_code(),
        /*network_service_path=*/esim_info1_->service_path(),
        /*state=*/hermes::profile::State::kPending,
        /*profile_class=*/hermes::profile::ProfileClass::kOperational,
        /*add_carrier_profile_behavior=*/
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);

    // Make Hermes operations take 1 second to complete.
    hermes_euicc_client->SetInteractiveDelay(base::Seconds(1));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateToCellularPage() {
    return Steps(
        Log("Navigating to the internet page"),

        NavigateSettingsToInternetPage(kOSSettingsId),

        Log("Waiting for cellular summary item to exist then click it"),

        WaitForElementExists(kOSSettingsId,
                             settings::cellular::CellularSummaryItem()),
        ClickElement(kOSSettingsId, settings::cellular::CellularSummaryItem()));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  OpenInstallationDialog() {
    return Steps(
        Log("Waiting for \"add eSIM\" button to be enabled then click it"),

        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::AddEsimButton()),
        ClickElement(kOSSettingsId, settings::cellular::AddEsimButton()),

        Log("Wait for the dialog to open"),

        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogTitle(),
            /*text=*/
            l10n_util::GetStringUTF8(
                IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_TITLE)));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep PerformSmdsSteps(
      const SimInfo& esim_info) {
    return Steps(
        Log("Overriding the profile returned by the first SM-DS scan"),

        Do([&]() {
          HermesEuiccClient::Get()
              ->GetTestInterface()
              ->SetNextRefreshSmdxProfilesResult(
                  {dbus::ObjectPath(esim_info.profile_path())});
        }),

        Log("Waiting to start the SM-DS scan"),

        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::EsimDialogForwardButton()),
        ClickElement(kOSSettingsId,
                     settings::cellular::EsimDialogForwardButton()),
        WaitForElementDisabled(kOSSettingsId,
                               settings::cellular::EsimDialogForwardButton()),

        Log("Wait for profiles to be discovered then choose one to install"),

        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogTitle(),
            /*text=*/
            l10n_util::GetStringUTF8(
                IDS_CELLULAR_SETUP_PROFILE_DISCOVERY_PAGE_TITLE)),
        WaitForElementHasAttribute(kOSSettingsId,
                                   settings::cellular::EsimDialogFirstProfile(),
                                   /*attribute=*/"selected"),
        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogFirstProfileLabel(),
            /*expected=*/esim_info.name()),
        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::EsimDialogForwardButton()),
        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogForwardButton(),
            /*text=*/
            l10n_util::GetStringUTF8(IDS_CELLULAR_SETUP_NEXT_LABEL)),
        ClickElement(kOSSettingsId,
                     settings::cellular::EsimDialogForwardButton()));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep PerformSmdpSteps(
      const SimInfo& esim_info) {
    return Steps(
        Log("Waiting to skip to manual entry"),

        WaitForElementExists(kOSSettingsId,
                             settings::cellular::EsimDialogSkipDiscoveryLink()),
        ClickElement(kOSSettingsId,
                     settings::cellular::EsimDialogSkipDiscoveryLink()),
        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogTitle(),
            /*text=*/
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_INTERNET_CELLULAR_SETUP_DIALOG_TITLE)),

        Log("Waiting to input the activation code"),

        WaitForElementExists(
            kOSSettingsId, settings::cellular::EsimDialogActivationCodeInput()),
        ClickElement(kOSSettingsId,
                     settings::cellular::EsimDialogActivationCodeInput()),
        SendTextAsKeyEvents(kOSSettingsId, esim_info.activation_code()),
        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::EsimDialogForwardButton()),
        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogForwardButton(),
            /*text=*/
            l10n_util::GetStringUTF8(IDS_CELLULAR_SETUP_NEXT_LABEL)),
        ClickElement(kOSSettingsId,
                     settings::cellular::EsimDialogForwardButton()));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep FinishInstallationFlow(
      const SimInfo& esim_info,
      const ui::test::StateIdentifier<WaitForServiceConnectedObserver>&
          state_identifier) {
    return Steps(
        Log("Wait for the installation to start"),

        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogInstallingMessage(),
            /*text=*/
            l10n_util::GetStringUTF8(
                IDS_CELLULAR_SETUP_ESIM_PROFILE_INSTALLING_MESSAGE)),

        Log("Wait for the Shill service to be created then connect to it"),

        WaitForState(state_identifier, true),

        Log("Wait for the installation to finish then close the dialog"),

        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogTitle(),
            /*text=*/
            l10n_util::GetStringUTF8(
                IDS_CELLULAR_SETUP_ESIM_FINAL_PAGE_SUCCESS_HEADER)),
        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::EsimDialogForwardButton()),
        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::EsimDialogForwardButton(),
            /*text=*/
            l10n_util::GetStringUTF8(IDS_CELLULAR_SETUP_DONE_LABEL)),
        ClickElement(kOSSettingsId,
                     settings::cellular::EsimDialogForwardButton()),

        WaitForElementDoesNotExist(kOSSettingsId,
                                   settings::cellular::EsimDialog()),

        Log("Wait for the installed profile to be visible in the UI"),

        WaitForAnyElementTextContains(
            kOSSettingsId, settings::cellular::EsimNetworkList(),
            WebContentsInteractionTestUtil::DeepQuery(
                {"network-list-item", "div#itemTitle"}),
            /*text=*/esim_info.nickname()));
  }

  const SimInfo& esim_info0() const { return *esim_info0_; }
  const SimInfo& esim_info1() const { return *esim_info1_; }

 private:
  std::unique_ptr<SimInfo> esim_info0_;
  std::unique_ptr<SimInfo> esim_info1_;
};

IN_PROC_BROWSER_TEST_F(EsimInstallationInteractiveUiTest, WithSmds) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      ObserveState(kConnectedToFirstCellularService,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info0().iccid())),

      ObserveState(kConnectedToSecondCellularService,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info1().iccid())),

      NavigateToCellularPage(),

      Log("Beginning first eSIM installation"),

      OpenInstallationDialog(),

      PerformSmdsSteps(esim_info0()),

      FinishInstallationFlow(esim_info0(), kConnectedToFirstCellularService),

      Log("Beginning second eSIM installation"),

      OpenInstallationDialog(),

      PerformSmdsSteps(esim_info1()),

      FinishInstallationFlow(esim_info1(), kConnectedToSecondCellularService),

      Log("Closing Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(EsimInstallationInteractiveUiTest, WithSmdp) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      ObserveState(kConnectedToFirstCellularService,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info0().iccid())),

      ObserveState(kConnectedToSecondCellularService,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info1().iccid())),

      NavigateToCellularPage(),

      Log("Beginning first eSIM installation"),

      OpenInstallationDialog(),

      PerformSmdpSteps(esim_info0()),

      FinishInstallationFlow(esim_info0(), kConnectedToFirstCellularService),

      Log("Beginning second eSIM installation"),

      OpenInstallationDialog(),

      PerformSmdpSteps(esim_info1()),

      FinishInstallationFlow(esim_info1(), kConnectedToSecondCellularService),

      Log("Closing Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
