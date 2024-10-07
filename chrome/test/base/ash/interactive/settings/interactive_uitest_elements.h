// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

namespace ash::settings {

// Top-level internet page.
WebContentsInteractionTestUtil::DeepQuery InternetPage();

// The error toast shown on the internet page.
WebContentsInteractionTestUtil::DeepQuery InternetPageErrorToast();

// The error toast shown on the internet page.
WebContentsInteractionTestUtil::DeepQuery InternetPageErrorToastMessage();

// The details subpage for a particular network.
WebContentsInteractionTestUtil::DeepQuery InternetDetailsSubpage();

// The "more options" / "three dots" button on the network details page.
WebContentsInteractionTestUtil::DeepQuery NetworkMoreDetailsMenuButton();

// The title of a internet settings subpage.
WebContentsInteractionTestUtil::DeepQuery InternetSettingsSubpageTitle();

// The network state shown on a settings subpage.
WebContentsInteractionTestUtil::DeepQuery SettingsSubpageNetworkState();

// The policy icon shown on a settings subpage.
WebContentsInteractionTestUtil::DeepQuery SettingsSubpagePolicyIcon();

// The network state shown on a settings subpage.
WebContentsInteractionTestUtil::DeepQuery SettingsSubpagePropertyList();

// The "Configure" button shown on a settings subpage.
WebContentsInteractionTestUtil::DeepQuery SettingsSubpageConfigureButton();

// The "Forget" button shown on a settings subpage.
WebContentsInteractionTestUtil::DeepQuery SettingsSubpageForgetButton();

// The "Connect" / "Disconnect" button shown on a settings subpage.
WebContentsInteractionTestUtil::DeepQuery
SettingsSubpageConnectDisconnectButton();

// The "Add connection" expand button shown on a settings subpage.
WebContentsInteractionTestUtil::DeepQuery AddConnectionsExpandButton();

// The "Add Wi-Fi" row in the expansion of "Add connection".
WebContentsInteractionTestUtil::DeepQuery AddWiFiRow();

// The "Add built-in VPN" row in the expansion of "Add connection".
WebContentsInteractionTestUtil::DeepQuery AddBuiltInVpnRow();

// The network config dialog.
WebContentsInteractionTestUtil::DeepQuery InternetConfigDialog();

// The title of the network config dialog.
WebContentsInteractionTestUtil::DeepQuery InternetConfigDialogTitle();

namespace cellular {

// The "add eSIM" button on the cellular page.
WebContentsInteractionTestUtil::DeepQuery AddEsimButton();

// The APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialog();

// The "Advanced Settings" button in APN details dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogAdvancedSettingsButton();

// The expandable group items under the "Advanced Settings" in APN details
// dialog.
WebContentsInteractionTestUtil::DeepQuery ApnDialogAdvancedSettingsGroup();

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

// The first APN item in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListFirstItem();

// The first APN item name in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListFirstItemName();

// The first APN item sublabel in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListFirstItemSublabel();

// The n-th APN item in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListNthItem(int n);

// The n-th APN item name in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListNthItemName(int n);

// The n-th APN item action menu button in APN list.
WebContentsInteractionTestUtil::DeepQuery ApnListNthItemMenuButton(int n);

// The n-th APN item action dots menu dialog of its APN item.
WebContentsInteractionTestUtil::DeepQuery ApnListNthItemDotsMenu(int n);

// The n-th APN item disable button in its dots menu.
WebContentsInteractionTestUtil::DeepQuery ApnListNthItemDisableButton(int n);

// The n-th APN item remove button in its dots menu.
WebContentsInteractionTestUtil::DeepQuery ApnListNthItemRemoveButton(int n);

// The n-th APN item enable button in its dots menu.
WebContentsInteractionTestUtil::DeepQuery ApnListNthItemEnableButton(int n);

// The div that indicates the auto-detected APN in the APN list item.
WebContentsInteractionTestUtil::DeepQuery ApnListItemAutoDetectedDiv();

// The confirm button in the APNs selection dialog.
WebContentsInteractionTestUtil::DeepQuery ApnSelectionConfirmButton();

// The APNs selection dialog.
WebContentsInteractionTestUtil::DeepQuery ApnSelectionDialog();

// The first APN in the APNs selection dialog.
WebContentsInteractionTestUtil::DeepQuery ApnSelectionFirstItem();

// The first APN in the APNs selection dialog.
WebContentsInteractionTestUtil::DeepQuery ApnSelectionFirstItemName();

// The action menu button in APN subpage.
WebContentsInteractionTestUtil::DeepQuery ApnSubpageActionMenuButton();

// The "Create new APN" button in the action menu in APN subpage.
WebContentsInteractionTestUtil::DeepQuery ApnSubpageCreateApnButton();

// The policy icon on the top of the APN subpage.
WebContentsInteractionTestUtil::DeepQuery ApnSubpagePolicyIcon();

// The "Show known APNs" button in the action menu in APN subpage.
WebContentsInteractionTestUtil::DeepQuery ApnSubpageShowKnownApnsButton();

// The "Zero" state banner in APN subpage when there're no APNs.
WebContentsInteractionTestUtil::DeepQuery ApnSubpageZeroStateContent();

// The cellular "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery CellularSummaryItem();

// The cellular inhibited element in Mobile data subpage.
WebContentsInteractionTestUtil::DeepQuery CellularInhibitedItem();

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

// The policy icon shown on the network list item row in cellular networks list.
WebContentsInteractionTestUtil::DeepQuery CellularNetworkListItemPolicyIcon();

// The cellular networks subpage pSIM networks list title.
WebContentsInteractionTestUtil::DeepQuery CellularSubpagePsimListTitle();

// The APN policy icon in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsSubpageApnPolicyIcon();

// The auto connect toggle in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery
CellularDetailsSubpageAutoConnectToggle();

// The allow data roaming togle in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery
CellularDetailsAllowDataRoamingToggle();

// The network operator property in the cellular network details subpage. The
// celluler network must be active for this to be shown.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsNetworkOperator();

// The advanced section row in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsAdvancedSection();

// The configurable section row in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsConfigurableSection();

// The proxy section row in cellular network details subpage.
WebContentsInteractionTestUtil::DeepQuery CellularDetailsProxySection();

// Elements related to SIM lock that are found within the advanced section of
// the details page of a cellular network, or the dialog shown to unlock a
// locked SIM either using a PIN or a PUK.
WebContentsInteractionTestUtil::DeepQuery CellularSimLockToggle();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockTogglePolicyIcon();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockChangePinButton();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockDialogs();
WebContentsInteractionTestUtil::DeepQuery
CellularSimLockEnterPinDialogPolicySubtitle();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockEnterPinDialogButton();
WebContentsInteractionTestUtil::DeepQuery
CellularSimLockEnterPinDialogSubtext();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockEnterPinDialogPin();
WebContentsInteractionTestUtil::DeepQuery
CellularSimLockChangePinDialogButton();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockChangePinDialogNew();
WebContentsInteractionTestUtil::DeepQuery
CellularSimLockChangePinDialogNewConfirm();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockChangePinDialogOld();
WebContentsInteractionTestUtil::DeepQuery
CellularSimLockUnlockPinDialogButton();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockUnlockPinDialogPin();
WebContentsInteractionTestUtil::DeepQuery
CellularSimLockUnlockPukDialogButton();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockUnlockPukDialogPin();
WebContentsInteractionTestUtil::DeepQuery CellularSimLockUnlockPukDialogPuk();
WebContentsInteractionTestUtil::DeepQuery
CellularSimLockUnlockPukDialogPinConfirm();

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

// Hotspot client count item in the hotspot detail page.
WebContentsInteractionTestUtil::DeepQuery HotspotClientCountItem();

}  // namespace hotspot

namespace wifi {

// The network list under the WiFi subpage.
WebContentsInteractionTestUtil::DeepQuery WifiNetworksList();

// The WiFi toggle in WiFi subpage page.
WebContentsInteractionTestUtil::DeepQuery WifiSubpageEnableToggle();

// The wifi "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery WifiSummaryItem();

// The "add Wi-Fi" button on the Wi-Fi subpage.
WebContentsInteractionTestUtil::DeepQuery AddWifiButton();

// The dialog opened when the "add Wi-Fi" button on the Wi-Fi subpage is
// clicked.
WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialog();

// The SSID input field on the "add Wi-Fi" dialog.
WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialogSsidInput();

// The "share this network" toggle on the "add Wi-Fi" dialog.
WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialogShareToggle();

// The connect button on the "add Wi-Fi" dialog.
WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialogConnectButton();

// The Known networks subpage button on the network page.
WebContentsInteractionTestUtil::DeepQuery WifiKnownNetworksSubpageButton();

// The known networks subpage.
WebContentsInteractionTestUtil::DeepQuery KnownNetworksSubpage();

// The passpoint subscription list on the known network subpage.
WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointSubsciptions();

// The passpoint subscription list item on the known network subpage passpoint
// section.
WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointSubscriptionItem();

// The dots button on the subscription item in the passpoint subscriptions
// section of the Known Networks subpage.
WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointMoreButton();

// The dots menu on the subscription item in the passpoint subscriptions
// section of the Known Networks subpage.
WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointDotsMenu();

// The Forget button in the dots menu on the subscription item in the passpoint
// subscriptions section of the Known Networks subpage.
WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointSubscriptionForget();

// The expiration date on the passpoint subscription subpage.
WebContentsInteractionTestUtil::DeepQuery PasspointSubpageExpirationDate();

// The source provider on the passpoint subscription subpage.
WebContentsInteractionTestUtil::DeepQuery PasspointSubpageProviderSource();

// The associated network list item on the passpoint subscription subpage.
WebContentsInteractionTestUtil::DeepQuery
PasspointSubpageAssociatedNetworksListItem();

// The domain list expansion button on the passpoint subscription subpage.
WebContentsInteractionTestUtil::DeepQuery
PasspointSubpageDomainExpansionButton();

// The domain list on the passpoint subscription subpage.
WebContentsInteractionTestUtil::DeepQuery PasspointSubpageDomainList();

// The domain item on the passpoint subscription subpage.
WebContentsInteractionTestUtil::DeepQuery PasspointSubpageDomainListItem();

// The Remove button on the passpoint subscription subpage.
WebContentsInteractionTestUtil::DeepQuery PasspointSubpageRemoveButton();

// The remove confirmation dialog when removing subscription.
WebContentsInteractionTestUtil::DeepQuery PasspointSubpageRemoveDialog();

// The confirm button on the subscription removal dialog.
WebContentsInteractionTestUtil::DeepQuery
PasspointSubpageRemoveDialogConfirmButton();

}  // namespace wifi

namespace vpn {

// The Service name input in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogServiceNameInput();

// The provider select button in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogProviderTypeSelect();

// The provider type option under the dropdown menu in the "Join VPN network"
// dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogProviderTypeOptions();

// The VPN host name input in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogHostnameInput();

// The OpenVPN user name input in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogOpenVpnUsernameInput();

// The OpenVPN password input in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogOpenVpnPasswordInput();

// The OpenVPN OTP input in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogOpenVpnOtpInput();

// The save credentails toggle in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogSaveCredentialsToggle();

// The "Connect" button in the "Join VPN network" dialog.
WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogConnectButton();

// The network list under the VPN subpage.
WebContentsInteractionTestUtil::DeepQuery VpnNetworksList();

// The first VPN item in built-in VPN list.
WebContentsInteractionTestUtil::DeepQuery VpnNetworksListFirstItem();

// The VPN provider type in the VPN subpage.
WebContentsInteractionTestUtil::DeepQuery VpnSubpageProviderType();

// The VPN host name input in the VPN subpage.
WebContentsInteractionTestUtil::DeepQuery VpnSubpageHostnameInput();

// The VPN username input in the VPN subpage.
WebContentsInteractionTestUtil::DeepQuery VpnSubpageUsernameInput();

// The vpn "row" on the top-level internet page.
WebContentsInteractionTestUtil::DeepQuery VpnSummaryItem();

}  // namespace vpn

namespace bluetooth {

// The top level Bluetooth page.
WebContentsInteractionTestUtil::DeepQuery BluetoothPage();

// The pair new device button in top level Bluetooth page.
WebContentsInteractionTestUtil::DeepQuery BluetoothPairNewDeviceButton();

// The Bluetooth pairing dialog.
WebContentsInteractionTestUtil::DeepQuery BluetoothPairingDialog();

// The Bluetooth device list page.
WebContentsInteractionTestUtil::DeepQuery BluetoothDeviceList();

// The Bluetooth state toggle in Bluetooth subpage.
WebContentsInteractionTestUtil::DeepQuery BluetoothSubpageToggle();

// The Bluetooth device details subpage.
WebContentsInteractionTestUtil::DeepQuery BluetoothDeviceDetailSubpage();

// The change name button in Bluetooth device details page.
WebContentsInteractionTestUtil::DeepQuery BluetoothChangeDeviceNameButton();

// The Battery percentage in Bluetooth device details page.
WebContentsInteractionTestUtil::DeepQuery BluetoothBatteryPercentage();

// The forget device button in Bluetooth device details page.
WebContentsInteractionTestUtil::DeepQuery BluetoothForgetDeviceButton();

// The Bluetooth name label in Bluetooth device details page.
WebContentsInteractionTestUtil::DeepQuery BluetoothDeviceName();

// The Bluetooth rename dialog.
WebContentsInteractionTestUtil::DeepQuery BluetoothRenameDialog();

// The Bluetooth rename dialog text input field.
WebContentsInteractionTestUtil::DeepQuery BluetoothRenameDialogInputField();

// The Bluetooth rename dialog done button.
WebContentsInteractionTestUtil::DeepQuery BluetoothRenameDialogDoneButton();

// The Bluetooth forget device dialog.
WebContentsInteractionTestUtil::DeepQuery BluetoothForgetDialog();

// The Bluetooth forget device dialog done button.
WebContentsInteractionTestUtil::DeepQuery BluetoothForgetDialogDoneButton();
}  // namespace bluetooth

}  // namespace ash::settings

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_SETTINGS_INTERACTIVE_UITEST_ELEMENTS_H_
