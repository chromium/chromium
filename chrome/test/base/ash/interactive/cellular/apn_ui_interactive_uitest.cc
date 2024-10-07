// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/cellular/wait_for_service_connected_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/fake_hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "dbus/object_path.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

const char kDefaultCustomApnName[] = "default_only_apn";
const char kAttachCustomApnName[] = "attach_only_apn";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

class ApnUiInteractiveUiTest : public EsimInteractiveUiTestBase {
 protected:
  ApnUiInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kAllowApnModificationPolicy);
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    esim_info_ = std::make_unique<SimInfo>(/*id=*/0);
    ConfigureEsimProfile(euicc_info(), *esim_info_, /*connected=*/true);

    default_modb_apn_name_ = *ShillServiceClient::Get()
                                  ->GetTestInterface()
                                  ->GetFakeDefaultModbApnDict()
                                  .FindString(shill::kApnNameProperty);
  }

  const SimInfo& esim_info() const { return *esim_info_; }
  const std::string& default_modb_apn_name() const {
    return default_modb_apn_name_;
  }

  void VerifyNoCustomApnsInShill() {
    const base::Value::Dict* cellular_properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            esim_info().service_path());
    ASSERT_TRUE(cellular_properties);
    const base::Value::List* shill_custom_apns =
        cellular_properties->FindList(shill::kCellularCustomApnListProperty);
    ASSERT_TRUE(shill_custom_apns);
    EXPECT_EQ(0u, shill_custom_apns->size());
  }

  void VerifyCustomApnInShill(bool expect_exists,
                              const std::string& expected_apn_name,
                              const std::string& expected_apn_type) {
    const base::Value::Dict* cellular_properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            esim_info().service_path());
    ASSERT_TRUE(cellular_properties);
    const base::Value::List* shill_custom_apns =
        cellular_properties->FindList(shill::kCellularCustomApnListProperty);
    ASSERT_TRUE(shill_custom_apns);

    bool found_matching_apn_name = false;
    for (const auto& custom_apn : *shill_custom_apns) {
      CHECK(custom_apn.is_dict());

      const std::string* apn_name =
          custom_apn.GetDict().FindString(shill::kApnProperty);
      if (!apn_name || *apn_name != expected_apn_name) {
        continue;
      }
      found_matching_apn_name = true;
      const std::string* apn_type =
          custom_apn.GetDict().FindString(shill::kApnTypesProperty);
      ASSERT_TRUE(apn_type);
      EXPECT_EQ(expected_apn_type, *apn_type);
    }
    EXPECT_EQ(expect_exists, found_matching_apn_name);
  }

  void VerifyLastGoodApn(const std::string expected_apn_name) {
    const base::Value::Dict* cellular_properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            esim_info().service_path());
    ASSERT_TRUE(cellular_properties);
    const base::Value::Dict* last_good_apn =
        cellular_properties->FindDict(shill::kCellularLastGoodApnProperty);
    ASSERT_TRUE(last_good_apn);
    const std::string* apn_name =
        last_good_apn->FindString(shill::kApnProperty);
    ASSERT_TRUE(apn_name);
    EXPECT_EQ(expected_apn_name, *apn_name);
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep CreateCustomApn(
      const std::string& apn_name,
      bool is_default,
      bool is_attach) {
    return Steps(
        Log("Open add custom APN dialog"),

        OpenAddCustomApnDetailsDialog(kOSSettingsId),

        Log("Type in custom APN name in APN dialog"),

        WaitForElementExists(kOSSettingsId,
                             settings::cellular::ApnDialogApnInput()),
        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::ApnDialogApnInput()),
        ClickElement(kOSSettingsId, settings::cellular::ApnDialogApnInput()),
        SendTextAsKeyEvents(kOSSettingsId, apn_name),

        Log("Expand advanced settings"),

        WaitForElementEnabled(
            kOSSettingsId,
            settings::cellular::ApnDialogAdvancedSettingsButton()),
        ClickElement(kOSSettingsId,
                     settings::cellular::ApnDialogAdvancedSettingsButton()),
        WaitForElementOpened(
            kOSSettingsId,
            settings::cellular::ApnDialogAdvancedSettingsGroup()),

        Log("Check APN type defaults to 'Default'"),

        WaitForElementChecked(kOSSettingsId,
                              settings::cellular::ApnDialogDefaultCheckbox()),
        WaitForElementUnchecked(kOSSettingsId,
                                settings::cellular::ApnDialogAttachCheckbox()),

        Log(base::StringPrintf("Select APN type: Default: %s, Attach: %s",
                               is_default ? "true" : "false",
                               is_attach ? "true" : "false")),

        SelectApnTypeInDialog(is_default, is_attach),

        Log("Save the custom APN"),

        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::ApnDialogAddActionButton()),
        ClickElement(kOSSettingsId,
                     settings::cellular::ApnDialogAddActionButton()),
        WaitForElementDoesNotExist(kOSSettingsId,
                                   settings::cellular::ApnDialog()));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep DisableNthApnInList(
      int n) {
    return ClickButtonInDotsMenuOfNthApnInList(
        n, settings::cellular::ApnListNthItemDisableButton(n));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep RemoveNthApnInList(
      int n) {
    return ClickButtonInDotsMenuOfNthApnInList(
        n, settings::cellular::ApnListNthItemRemoveButton(n));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep EnableNthApnInList(
      int n) {
    return ClickButtonInDotsMenuOfNthApnInList(
        n, settings::cellular::ApnListNthItemEnableButton(n));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForApnConnected(
      const std::string& apn_name) {
    return Steps(
        WaitForElementTextContains(kOSSettingsId,
                                   settings::cellular::ApnListFirstItemName(),
                                   /*text=*/apn_name),
        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::ApnListFirstItemSublabel(),
            /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),
        Do([&]() { VerifyLastGoodApn(apn_name); }));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForAutoDetectedApnConnected() {
    return Steps(
        WaitForApnConnected(default_modb_apn_name()),
        WaitForElementTextContains(
            kOSSettingsId, settings::cellular::ApnListItemAutoDetectedDiv(),
            /*text=*/
            l10n_util::GetStringUTF8(IDS_SETTINGS_APN_AUTO_DETECTED).c_str()));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForApnPolicyApplied(
      bool allow_apn_modification) {
    return Steps(Do([allow_apn_modification]() {
      base::Value::Dict global_config;
      global_config.Set(::onc::global_network_config::kAllowAPNModification,
                        allow_apn_modification);
      NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
          ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
          base::Value::List(), global_config);
    }));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  VerifyApnModificationIsRestricted() {
    return Steps(
        Log("Wait for policy icon in cellular detail subpage"),

        WaitForElementExists(
            kOSSettingsId,
            settings::cellular::CellularDetailsSubpageApnPolicyIcon()),

        Log("Navigate to the APN revamp details page"),

        NavigateToApnRevampDetailsPage(kOSSettingsId),

        Log("Wait for APN action button is disabled"),

        WaitForElementExists(kOSSettingsId,
                             settings::cellular::ApnSubpagePolicyIcon()));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  DisableAndEnableMobileData() {
    return Steps(
        Log("Re-enabling mobile data"),

        NavigateSettingsToInternetPage(kOSSettingsId),
        WaitForToggleState(kOSSettingsId,
                           settings::cellular::MobileDataToggle(), true),

        Log("Disabling mobile data"),

        ClickElement(kOSSettingsId, settings::cellular::MobileDataToggle()),
        WaitForToggleState(kOSSettingsId,
                           settings::cellular::MobileDataToggle(), false),

        Log("Enabling mobile data"),

        ClickElement(kOSSettingsId, settings::cellular::MobileDataToggle()),
        WaitForToggleState(kOSSettingsId,
                           settings::cellular::MobileDataToggle(), true));
  }

 private:
  ui::test::internal::InteractiveTestPrivate::MultiStep
  ClickButtonInDotsMenuOfNthApnInList(
      int n,
      const WebContentsInteractionTestUtil::DeepQuery& action_button) {
    return Steps(
        WaitForElementEnabled(kOSSettingsId,
                              settings::cellular::ApnListNthItemMenuButton(n)),
        ClickElement(kOSSettingsId,
                     settings::cellular::ApnListNthItemMenuButton(n)),
        WaitForElementOpened(kOSSettingsId,
                             settings::cellular::ApnListNthItemDotsMenu(n)),
        WaitForElementEnabled(kOSSettingsId, action_button),
        ClickElement(kOSSettingsId, action_button),
        WaitForElementUnopened(kOSSettingsId,
                               settings::cellular::ApnListNthItemDotsMenu(n)));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep SelectApnTypeInDialog(
      bool select_default_type,
      bool select_attach_type) {
    if (select_default_type && !select_attach_type) {
      return Steps();
    }
    if (select_default_type && select_attach_type) {
      return Steps(
          SelectCheckbox(settings::cellular::ApnDialogAttachCheckbox()));
    }
    if (!select_default_type && select_attach_type) {
      return Steps(
          UnselectCheckbox(settings::cellular::ApnDialogDefaultCheckbox()),
          SelectCheckbox(settings::cellular::ApnDialogAttachCheckbox()));
    }
    // APN type is required.
    NOTREACHED();
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep SelectCheckbox(
      const WebContentsInteractionTestUtil::DeepQuery& checkbox) {
    return Steps(WaitForElementUnchecked(kOSSettingsId, checkbox),
                 ClickElement(kOSSettingsId, checkbox),
                 WaitForElementChecked(kOSSettingsId, checkbox));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep UnselectCheckbox(
      const WebContentsInteractionTestUtil::DeepQuery& checkbox) {
    return Steps(WaitForElementChecked(kOSSettingsId, checkbox),
                 ClickElement(kOSSettingsId, checkbox),
                 WaitForElementUnchecked(kOSSettingsId, checkbox));
  }

  std::unique_ptr<SimInfo> esim_info_;
  std::string default_modb_apn_name_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest,
                       NonConnectedCellularHasNoApn) {
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

      WaitForAutoDetectedApnConnected(),

      Log("Disconnect cellular network"),

      Do([&]() { esim_info().Disconnect(); }),

      Log("Verify Zero state message shows and no APN shows in the list when "
          "not connected"),

      WaitForElementExists(kOSSettingsId,
                           settings::cellular::ApnSubpageZeroStateContent()),
      WaitForElementHasAttribute(
          kOSSettingsId, settings::cellular::ApnListFirstItem(), "hidden"),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest, MultipleNetworksDefaultApn) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                      kConnectedToCellularService);
  SimInfo esim_info1(/*id=*/1);
  ConfigureEsimProfile(euicc_info(), esim_info1, /*connected=*/false);

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

      WaitForAutoDetectedApnConnected(),

      Log("Navigate to the second cellular network"),

      NavigateToInternetDetailsPage(
          kOSSettingsId, NetworkTypePattern::Cellular(), esim_info1.nickname()),

      Log("Connect to the second network"),

      ObserveState(kConnectedToCellularService,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info1.iccid())),
      Do([&]() { esim_info1.Connect(); }),
      WaitForState(kConnectedToCellularService, true),

      Log("Wait for the UI shows connected"),

      WaitForElementTextContains(
          kOSSettingsId, settings::SettingsSubpageNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),

      Log("Navigate to the second APN revamp details page"),

      NavigateToApnRevampDetailsPage(kOSSettingsId),

      Log("Verify it connects to the auto detected APN"),

      WaitForAutoDetectedApnConnected(),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest, CreateDefaultCustomApn) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Verify no custom APNs before start testing"),

      Do([&]() { VerifyNoCustomApnsInShill(); }),

      Log("Navigating to the internet page"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Navigate to the APN revamp details page"),

      NavigateToApnRevampDetailsPage(kOSSettingsId),

      Log("Create default custom APN from the dialog"),

      CreateCustomApn(kDefaultCustomApnName, /*is_default=*/true,
                      /*is_attach=*/false),

      Log("Wait for the newly created custom APN appear at the top of the "
          "list"),

      WaitForElementExists(kOSSettingsId,
                           settings::cellular::ApnListFirstItem()),
      WaitForElementTextContains(kOSSettingsId,
                                 settings::cellular::ApnListFirstItemName(),
                                 /*text=*/kDefaultCustomApnName),

      Log("Verify the custom APN saved in Shill"),

      Do([&]() {
        VerifyCustomApnInShill(/*expect_exists=*/true, kDefaultCustomApnName,
                               shill::kApnTypeDefault);
      }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest,
                       CreateDefaultAndAttachCustomApn) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Verify no custom APNs before start testing"),

      Do([&]() { VerifyNoCustomApnsInShill(); }),

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
      SendTextAsKeyEvents(kOSSettingsId, kDefaultCustomApnName),

      Log("Expand advanced settings"),

      WaitForElementEnabled(
          kOSSettingsId, settings::cellular::ApnDialogAdvancedSettingsButton()),
      ClickElement(kOSSettingsId,
                   settings::cellular::ApnDialogAdvancedSettingsButton()),
      WaitForElementOpened(
          kOSSettingsId, settings::cellular::ApnDialogAdvancedSettingsGroup()),

      Log("Check APN type defaults to 'Default'"),

      WaitForElementChecked(kOSSettingsId,
                            settings::cellular::ApnDialogDefaultCheckbox()),

      Log("Uncheck 'Default' type"),

      ClickElement(kOSSettingsId,
                   settings::cellular::ApnDialogDefaultCheckbox()),
      WaitForElementUnchecked(kOSSettingsId,
                              settings::cellular::ApnDialogDefaultCheckbox()),

      Log("Check 'Attach' type"),

      WaitForElementUnchecked(kOSSettingsId,
                              settings::cellular::ApnDialogAttachCheckbox()),
      ClickElement(kOSSettingsId,
                   settings::cellular::ApnDialogAttachCheckbox()),
      WaitForElementChecked(kOSSettingsId,
                            settings::cellular::ApnDialogAttachCheckbox()),

      Log("Check for the warning message"),

      WaitForElementExists(
          kOSSettingsId, settings::cellular::ApnDialogDefaultApnRequiredInfo()),

      Log("Check for the 'Add' button disabled"),

      WaitForElementDisabled(kOSSettingsId,
                             settings::cellular::ApnDialogAddActionButton()),

      Log("Re-check the 'Default' type"),

      ClickElement(kOSSettingsId,
                   settings::cellular::ApnDialogDefaultCheckbox()),
      WaitForElementChecked(kOSSettingsId,
                            settings::cellular::ApnDialogDefaultCheckbox()),

      Log("Save the custom APN"),

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
                                 /*text=*/kDefaultCustomApnName),

      Log("Wait for the custom APN connected"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::ApnListFirstItemSublabel(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),

      Log("Wait for the custom APN connected"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::ApnListFirstItemSublabel(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),

      Log("Verify the custom APN saved in Shill"),

      Do([&]() {
        VerifyCustomApnInShill(
            /*expect_exists*/ true, kDefaultCustomApnName,
            base::StringPrintf("%s,%s", shill::kApnTypeDefault,
                               shill::kApnTypeIA));
      }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest, DiscoverApns) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Verify no custom APNs before start testing"),

      Do([&]() { VerifyNoCustomApnsInShill(); }),

      Log("Navigating to the internet page"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Navigate to the APN revamp details page"),

      NavigateToApnRevampDetailsPage(kOSSettingsId),

      Log("Create default custom APN from the dialog"),

      CreateCustomApn(kDefaultCustomApnName, /*is_default=*/true,
                      /*is_attach=*/false),

      Log("Wait for the newly created custom APN appear at the top of the "
          "list"),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::cellular::ApnListFirstItemName(),
                                 /*text=*/kDefaultCustomApnName),

      Log("Wait for the custom APN connected"),

      WaitForApnConnected(kDefaultCustomApnName),

      Log("Open 'Show known APNs' dialog"),

      OpenApnSelectionDialog(kOSSettingsId),

      Log("Checking the name of the first APN item"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::ApnSelectionFirstItemName(),
          /*text=*/default_modb_apn_name()),

      Log("Select default APNs"),

      WaitForElementDisabled(kOSSettingsId,
                             settings::cellular::ApnSelectionConfirmButton()),
      ClickElement(kOSSettingsId, settings::cellular::ApnSelectionFirstItem()),
      WaitForElementEnabled(kOSSettingsId,
                            settings::cellular::ApnSelectionConfirmButton()),
      ClickElement(kOSSettingsId,
                   settings::cellular::ApnSelectionConfirmButton()),

      Log("Checking for the APN selection dialog close"),

      WaitForElementDoesNotExist(kOSSettingsId,
                                 settings::cellular::ApnSelectionDialog()),

      Log("Wait for the default APN connected"),

      WaitForApnConnected(default_modb_apn_name()),

      Log("Check the custom APN re-ordered to second APN"),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::cellular::ApnListNthItemName(2),
                                 /*text=*/kDefaultCustomApnName),

      Log("Check custom APN disabled"),

      WaitForElementHasAttribute(
          kOSSettingsId, settings::cellular::ApnListNthItem(2), "is-disabled_"),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest, EnableDisableRemoveApns) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Verify no custom APNs before start testing"),

      Do([&]() { VerifyNoCustomApnsInShill(); }),

      Log("Navigating to the internet page"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Navigate to the APN revamp details page"),

      NavigateToApnRevampDetailsPage(kOSSettingsId),

      Log("Create a default custom APN from the dialog"),

      CreateCustomApn(kDefaultCustomApnName, /*is_default=*/true,
                      /*is_attach=*/false),

      Log("Wait for the newly created custom APN appear at the top of the "
          "list"),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::cellular::ApnListFirstItemName(),
                                 /*text=*/kDefaultCustomApnName),

      Log("Wait for the default custom APN connected"),

      WaitForApnConnected(kDefaultCustomApnName),

      Log("Create a attach custom APN from the dialog"),

      CreateCustomApn(kAttachCustomApnName, /*is_default=*/false,
                      /*is_attach=*/true),

      Log("Checking for the default custom APN still connected"),

      WaitForApnConnected(kDefaultCustomApnName),

      Log("Attempt to disable Default APN"),

      DisableNthApnInList(1),

      Log("Checking for a warning toast and default APN not disabled in Shill"),

      WaitForElementTextContains(
          kOSSettingsId, settings::InternetPageErrorToastMessage(),
          l10n_util::GetStringUTF8(
              IDS_SETTINGS_APN_WARNING_PROMPT_FOR_DISABLE_REMOVE)
              .c_str()),
      Do([&]() {
        VerifyCustomApnInShill(/*expect_exists=*/true, kDefaultCustomApnName,
                               shill::kApnTypeDefault);
      }),

      Log("Attempt to remove Default APN"),

      RemoveNthApnInList(1),

      Log("Checking for a warning toast and default APN not removed in Shill"),

      WaitForElementTextContains(
          kOSSettingsId, settings::InternetPageErrorToastMessage(),
          l10n_util::GetStringUTF8(
              IDS_SETTINGS_APN_WARNING_PROMPT_FOR_DISABLE_REMOVE)
              .c_str()),
      Do([&]() {
        VerifyCustomApnInShill(/*expect_exists=*/true, kDefaultCustomApnName,
                               shill::kApnTypeDefault);
      }),

      Log("Disable Attach APN"),

      DisableNthApnInList(2),
      WaitForElementHasAttribute(
          kOSSettingsId, settings::cellular::ApnListNthItem(2), "is-disabled_"),
      Do([&]() {
        VerifyCustomApnInShill(/*expect_exists=*/false, kAttachCustomApnName,
                               shill::kApnTypeIA);
      }),

      Log("Disable Default APN"),

      DisableNthApnInList(1),

      Log("Checking for auto-detected APN popped up in the APN list"),

      WaitForAutoDetectedApnConnected(),

      Log("Checking for Default custom APN removed and Modb APN connected in "
          "Shill"),

      Do([&]() { VerifyLastGoodApn(default_modb_apn_name()); }),

      Log("Checking for both Default and Attach APNs disabled"),

      WaitForElementHasAttribute(
          kOSSettingsId, settings::cellular::ApnListNthItem(2), "is-disabled_"),
      WaitForElementHasAttribute(
          kOSSettingsId, settings::cellular::ApnListNthItem(3), "is-disabled_"),
      Do([&]() { VerifyNoCustomApnsInShill(); }),

      Log("Attempt to enable Attach APN"),

      EnableNthApnInList(2),

      Log("Checking for a warning toast"),

      WaitForElementTextContains(
          kOSSettingsId, settings::InternetPageErrorToastMessage(),
          l10n_util::GetStringUTF8(IDS_SETTINGS_APN_WARNING_PROMPT_FOR_ENABLE)
              .c_str()),
      Do([&]() {
        VerifyCustomApnInShill(/*expect_exists=*/false, kAttachCustomApnName,
                               shill::kApnTypeIA);
      }),

      Log("Enable Default APN"),

      EnableNthApnInList(3), WaitForApnConnected(kDefaultCustomApnName),
      Do([&]() {
        VerifyCustomApnInShill(/*expect_exists=*/true, kDefaultCustomApnName,
                               shill::kApnTypeDefault);
      }),

      Log("Remove Attach APN"),

      RemoveNthApnInList(2),

      Log("Remove Default APN"),

      RemoveNthApnInList(1),

      Log("Checking for auto-detected APN popped up in the APN list when "
          "there is no custom APN"),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::cellular::ApnListFirstItemName(),
                                 /*text=*/default_modb_apn_name()),
      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::ApnListFirstItemSublabel(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),
      Do([&]() { VerifyLastGoodApn(default_modb_apn_name()); }),

      Log("Checking for both custom APNs removed in Shill"),

      Do([&]() { VerifyNoCustomApnsInShill(); }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ApnUiInteractiveUiTest, ApnPolicy) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Verify no custom APNs before start testing"),

      Do([&]() { VerifyNoCustomApnsInShill(); }),

      Log("Navigating to the cellular detail subpage"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),
      WaitForElementDoesNotExist(
          kOSSettingsId,
          settings::cellular::CellularDetailsSubpageApnPolicyIcon()),

      Log("Prohibiting APN modification with policy"),

      WaitForApnPolicyApplied(/*allow_apn_modification=*/false),
      VerifyApnModificationIsRestricted(),

      DisableAndEnableMobileData(),

      Log("Navigating to the cellular detail subpage"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      VerifyApnModificationIsRestricted(),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
