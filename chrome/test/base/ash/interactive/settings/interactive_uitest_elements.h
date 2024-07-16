// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

namespace ash::settings {

// Top-level internet page.
WebContentsInteractionTestUtil::DeepQuery InternetPage();

// The "more options" / "three dots" button on the network details page.
WebContentsInteractionTestUtil::DeepQuery NetworkMoreDetailsMenuButton();

// The title of a settings subpage.
WebContentsInteractionTestUtil::DeepQuery SettingsSubpageTitle();

namespace cellular {

// The "add eSIM" button on the cellular page.
WebContentsInteractionTestUtil::DeepQuery AddEsimButton();

// The APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialog();

// The "Advanced Settings" button in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogAdvancedSettingsButton();

// The "Attach" checkbox of APN types in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogAttachCheckbox();

// The "Default" checkbox of APN types in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogDefaultCheckbox();

// The default APN required info message in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogDefaultApnRequiredInfo();

// The "Add" action button in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogAddActionButton();

// The APN name input in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogApnInput();

// The username input in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogUsernameInput();

// The password input in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogPasswordInput();

// The first apn item in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListFirstItem();

// The first apn item name in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListFirstItemName();

// The action menu button in APN subpage.
WebContentsInteractionTestUtil::DeepQuery ApnSubpageActionMenuButton();

// The "Create new APN" button in the action menu in APN subpage.
WebContentsInteractionTestUtil::DeepQuery ApnSubpageCreateApnButton();

// The "Zero" state banner in APN subpage when there're no APNs.
WebContentsInteractionTestUtil::DeepQuery ApnSubpageZeroStateContent();

// The cellular "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery CellularSummaryItem();

// The "add eSIM" dialog.
WebContentsInteractionTestUtil::DeepQuery EsimDialog();

// The activation code input field on the manual entry page of the "add eSIM"
// dialog.
WebContentsInteractionTestUtil::DeepQuery EsimDialogActivationCodeInput();

// The first eSIM profile in the list of eSIM profiles found via an SM-DS scan.
WebContentsInteractionTestUtil::DeepQuery EsimDialogFirstProfile();

// The label of the first eSIM profile in the list of eSIM profiles found via an
// SM-DS scan.
WebContentsInteractionTestUtil::DeepQuery EsimDialogFirstProfileLabel();

// The "forward" button of the "add eSIM" dialog. When pressed, this button will
// either navigate the user forward in the eSIM installation flow.
WebContentsInteractionTestUtil::DeepQuery EsimDialogForwardButton();

// The message shown to the user that provides information about the current
// step of the eSIM installation flow.
WebContentsInteractionTestUtil::DeepQuery EsimDialogInstallingMessage();

// The "skip discovery" / "manual entry" link of the "add eSIM" dialog. When
// pressed, this link will cause the eSIM installation flow to skip to the page
// where users can manually enter an activation code.
WebContentsInteractionTestUtil::DeepQuery EsimDialogSkipDiscoveryLink();

// The title of the "add eSIM" dialog.
WebContentsInteractionTestUtil::DeepQuery EsimDialogTitle();

// The list of eSIM networks.
WebContentsInteractionTestUtil::DeepQuery EsimNetworkList();

// The list of pSIM networks.
WebContentsInteractionTestUtil::DeepQuery PsimNetworkList();

// The Mobile data toggle on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery MobileDataToggle();

// The cellular networks list page.
WebContentsInteractionTestUtil::DeepQuery CellularNetworksList();

// The cellular networks subpage pSIM networks list title.
WebContentsInteractionTestUtil::DeepQuery CellularSubpagePsimListTitle();

// The cellular network details subpage title.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsSubpageTitle();

// The auto connect toggle in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery
CellularDetailsSubpageAutoConnectToggle();

// The allow data roaming togle in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery
CellularDetailsAllowDataRoamingToggle();

// The advanced setion row in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsAdvancedSection();

// The configurable setion row in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsConfigurableSection();

// The proxy setion row in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsProxySection();

// The cellular networks subpage menu rename button.
WebContentsInteractionTestUtil::DeepQuery CellularSubpageMenuRenameButton();

// The cellular networks subpage rename dialog dialog.
WebContentsInteractionTestUtil::DeepQuery CellularSubpageMenuRenameDialog();

// The cellular networks subpage rename dialog dialog "done" button.
WebContentsInteractionTestUtil::DeepQuery
CellularSubpageMenuRenameDialogDoneButton();

// The cellular networks subpage rename dialog dialog text "input" field.
WebContentsInteractionTestUtil::DeepQuery
CellularSubpageMenuRenameDialogInputField();

// The cellular "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery CellularSummaryItem();

// The apn "row" on the cellular network subpage, only available in apn revamp.
WebContentsInteractionTestUtil::DeepQuery CellularSubpageApnRow();

}  // namespace cellular

namespace ethernet {
// The ethernet "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery EthernetSummaryItem();
}  // namespace ethernet

namespace hotspot {

// The hotspot "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery HotspotSummaryItem();

// The link under the hotspot "row".
WebContentsInteractionTestUtil::DeepQuery HotspotSummarySubtitleLink();

// The hotspot toggle in the hotspot "row" of the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery HotspotToggle();

// The hotspot row's policy icon in cases where usage is restricted.
WebContentsInteractionTestUtil::DeepQuery HotspotPolicyIcon();

// The hotspot config dialog root.
WebContentsInteractionTestUtil::DeepQuery HotspotConfigDialog();

// The configure button within the hotspot detail page.
WebContentsInteractionTestUtil::DeepQuery HotspotConfigureButton();

// The save button on hotspot config dialog.
WebContentsInteractionTestUtil::DeepQuery HotspotDialogSaveButton();

// Hotspot SSID in the hotspot detail page.
WebContentsInteractionTestUtil::DeepQuery HotspotSSID();

// Hotspot SSID input in the hotspot config dialog.
WebContentsInteractionTestUtil::DeepQuery HotspotSSIDInput();

}  // namespace hotspot

namespace wifi {
// The wifi "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery WifiSummaryItem();
}  // namespace wifi

namespace vpn {
// The vpn "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery VpnSummaryItem();
}  // namespace vpn

}  // namespace ash::settings

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_
