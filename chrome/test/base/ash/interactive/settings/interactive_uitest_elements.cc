// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"

namespace ash::settings {

WebContentsInteractionTestUtil::DeepQuery InternetPage() {
  return WebContentsInteractionTestUtil::DeepQuery({{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-internet-page",
  }});
}

WebContentsInteractionTestUtil::DeepQuery InternetPageErrorToast() {
  return InternetPage() + "cr-toast";
}

WebContentsInteractionTestUtil::DeepQuery InternetPageErrorToastMessage() {
  return InternetPage() + "span#errorToastMessage";
}

WebContentsInteractionTestUtil::DeepQuery InternetDetailsSubpage() {
  return InternetPage() + "settings-internet-detail-subpage";
}

WebContentsInteractionTestUtil::DeepQuery NetworkMoreDetailsMenuButton() {
  return InternetPage() + "settings-internet-detail-menu" +
         "cr-icon-button#moreNetworkDetail";
}

WebContentsInteractionTestUtil::DeepQuery InternetSettingsSubpageTitle() {
  return InternetPage() + "os-settings-subpage.iron-selected" +
         "h1#subpageTitle";
}

WebContentsInteractionTestUtil::DeepQuery SettingsSubpageNetworkState() {
  return InternetDetailsSubpage() + "div#networkState";
}

WebContentsInteractionTestUtil::DeepQuery SettingsSubpagePolicyIcon() {
  return InternetDetailsSubpage() + "div#titleDiv" + "cr-policy-indicator";
}

WebContentsInteractionTestUtil::DeepQuery SettingsSubpageConfigureButton() {
  return InternetDetailsSubpage() + "cr-button#configureButton";
}

WebContentsInteractionTestUtil::DeepQuery SettingsSubpageForgetButton() {
  return InternetDetailsSubpage() + "cr-button#forgetButton";
}

WebContentsInteractionTestUtil::DeepQuery
SettingsSubpageConnectDisconnectButton() {
  return InternetDetailsSubpage() + "controlled-button#connectDisconnect" +
         "cr-button";
}

WebContentsInteractionTestUtil::DeepQuery SettingsSubpageBackButton() {
  return InternetDetailsSubpage() + "cr-button#backButton";
}

WebContentsInteractionTestUtil::DeepQuery AddConnectionsExpandButton() {
  return InternetPage() + "cr-expand-button#expandAddConnections";
}

WebContentsInteractionTestUtil::DeepQuery AddWiFiRow() {
  return InternetPage() + "div#add-wifi-label";
}

WebContentsInteractionTestUtil::DeepQuery AddBuiltInVpnRow() {
  return InternetPage() + "div#add-vpn-label";
}

WebContentsInteractionTestUtil::DeepQuery InternetConfigDialog() {
  return InternetPage() + "internet-config#configDialog" +
         "network-config#networkConfig";
}

WebContentsInteractionTestUtil::DeepQuery InternetConfigDialogTitle() {
  return InternetPage() + "internet-config#configDialog" + "div#dialogTitle";
}

namespace cellular {

WebContentsInteractionTestUtil::DeepQuery ApnDialog() {
  return InternetPage() + "apn-subpage" + "apn-list" + "apn-detail-dialog";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogAdvancedSettingsButton() {
  return ApnDialog() + "cr-expand-button";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogAdvancedSettingsGroup() {
  return ApnDialog() + "iron-collapse";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogAttachCheckbox() {
  return ApnDialog() + "cr-checkbox#apnAttachTypeCheckbox";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogDefaultCheckbox() {
  return ApnDialog() + "cr-checkbox#apnDefaultTypeCheckbox";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogAddActionButton() {
  return ApnDialog() + "cr-button#apnDetailActionBtn";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogApnInput() {
  return ApnDialog() + "cr-input#apnInput";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogDefaultApnRequiredInfo() {
  return ApnDialog() + "div#defaultApnRequiredInfo";
}

WebContentsInteractionTestUtil::DeepQuery ApnListFirstItem() {
  return InternetPage() + "apn-subpage" + "apn-list" +
         "apn-list-item:first-of-type";
}

WebContentsInteractionTestUtil::DeepQuery ApnListFirstItemName() {
  return ApnListFirstItem() + "div#apnName";
}

WebContentsInteractionTestUtil::DeepQuery ApnListFirstItemSublabel() {
  return ApnListFirstItem() + "div#subLabel";
}

WebContentsInteractionTestUtil::DeepQuery ApnListNthItem(int n) {
  return InternetPage() + "apn-subpage" + "apn-list" +
         base::StringPrintf("apn-list-item:nth-of-type(%u)", n);
}

WebContentsInteractionTestUtil::DeepQuery ApnListNthItemName(int n) {
  return ApnListNthItem(n) + "div#apnName";
}

WebContentsInteractionTestUtil::DeepQuery ApnListNthItemMenuButton(int n) {
  return ApnListNthItem(n) + "cr-icon-button#actionMenuButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnListNthItemDotsMenu(int n) {
  return ApnListNthItem(n) + "cr-action-menu#dotsMenu";
}

WebContentsInteractionTestUtil::DeepQuery ApnListNthItemDisableButton(int n) {
  return ApnListNthItem(n) + "button#disableButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnListNthItemRemoveButton(int n) {
  return ApnListNthItem(n) + "button#removeButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnListNthItemEnableButton(int n) {
  return ApnListNthItem(n) + "button#enableButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnListItemAutoDetectedDiv() {
  return ApnListFirstItem() + "div#autoDetected";
}

WebContentsInteractionTestUtil::DeepQuery ApnSelectionConfirmButton() {
  return ApnSelectionDialog() + "cr-button#apnSelectionActionBtn";
}

WebContentsInteractionTestUtil::DeepQuery ApnSelectionDialog() {
  return InternetPage() + "apn-subpage" + "apn-list" + "apn-selection-dialog";
}

WebContentsInteractionTestUtil::DeepQuery ApnSelectionFirstItem() {
  return ApnSelectionDialog() + "apn-selection-dialog-list-item:first-of-type";
}

WebContentsInteractionTestUtil::DeepQuery ApnSelectionFirstItemName() {
  return ApnSelectionFirstItem() + "span#friendlyApnName";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpageActionMenuButton() {
  return InternetPage() + "cr-icon-button#apnActionMenuButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpageCreateApnButton() {
  return InternetPage() + "button#createCustomApnButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpagePolicyIcon() {
  return InternetPage() + "cr-tooltip-icon#apnManagedIcon";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpageShowKnownApnsButton() {
  return InternetPage() + "button#discoverMoreApnsButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpageZeroStateContent() {
  return InternetPage() + "apn-subpage" + "apn-list" + "div#zeroStateContent";
}

WebContentsInteractionTestUtil::DeepQuery CellularSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#Cellular" +
         "div#networkSummaryItemRow";
}

WebContentsInteractionTestUtil::DeepQuery CellularInhibitedItem() {
  return InternetPage() + "settings-internet-subpage" +
         "cellular-networks-list" + "div#inhibitedSubtext";
}

WebContentsInteractionTestUtil::DeepQuery AddEsimButton() {
  return InternetPage() + "settings-internet-subpage" +
         "cellular-networks-list" + "cr-icon-button#addESimButton";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialog() {
  return InternetPage() + "os-settings-cellular-setup-dialog" +
         "cellular-setup" + "esim-flow-ui";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogActivationCodeInput() {
  return EsimDialog() + "activation-code-page" + "cr-input#activationCode" +
         "input#input";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogFirstProfile() {
  return EsimDialog() + "profile-discovery-list-page" +
         "profile-discovery-list-item:first-of-type";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogFirstProfileLabel() {
  return EsimDialogFirstProfile() + "div#profileTitleLabel";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogForwardButton() {
  return InternetPage() + "os-settings-cellular-setup-dialog" +
         "cellular-setup" + "button-bar" + "cr-button#forward";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogInstallingMessage() {
  return EsimDialog() + "setup-loading-page#profileInstallingPage" +
         "base-page" + "div#message";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogSkipDiscoveryLink() {
  return EsimDialog() + "profile-discovery-consent-page" +
         "localized-link#shouldSkipDiscovery" + "a";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogTitle() {
  return InternetPage() + "os-settings-cellular-setup-dialog" + "div#header";
}

WebContentsInteractionTestUtil::DeepQuery EsimNetworkList() {
  return InternetPage() + "settings-internet-subpage" +
         "cellular-networks-list" + "network-list#esimNetworkList";
}

WebContentsInteractionTestUtil::DeepQuery PsimNetworkList() {
  return InternetPage() + "settings-internet-subpage" +
         "cellular-networks-list" + "network-list#psimNetworkList";
}

WebContentsInteractionTestUtil::DeepQuery MobileDataToggle() {
  return InternetPage() + "network-summary" + "network-summary-item#Cellular" +
         "cr-toggle#deviceEnabledButton";
}

WebContentsInteractionTestUtil::DeepQuery CellularNetworksList() {
  return InternetPage() + "settings-internet-subpage" +
         "cellular-networks-list";
}

WebContentsInteractionTestUtil::DeepQuery CellularNetworkListItemPolicyIcon() {
  return CellularNetworksList() + "network-list" + "network-list-item" +
         "cr-policy-indicator";
}

WebContentsInteractionTestUtil::DeepQuery CellularSubpagePsimListTitle() {
  return CellularNetworksList() + "div#pSimLabel";
}

WebContentsInteractionTestUtil::DeepQuery
CellularDetailsSubpageApnPolicyIcon() {
  return InternetDetailsSubpage() + "cr-policy-indicator#apnManagedIcon";
}

WebContentsInteractionTestUtil::DeepQuery
CellularDetailsSubpageAutoConnectToggle() {
  return InternetDetailsSubpage() + "settings-toggle-button#autoConnectToggle";
}

WebContentsInteractionTestUtil::DeepQuery
CellularDetailsAllowDataRoamingToggle() {
  return InternetDetailsSubpage() + "cellular-roaming-toggle-button";
}

WebContentsInteractionTestUtil::DeepQuery CellularDetailsNetworkOperator() {
  return InternetDetailsSubpage() + "network-property-list-mojo#infoFields" +
         "div#cellular\\.servingOperator\\.name";
}

WebContentsInteractionTestUtil::DeepQuery CellularDetailsAdvancedSection() {
  return InternetDetailsSubpage() + "cr-expand-button#advancedSectionToggle";
}

WebContentsInteractionTestUtil::DeepQuery CellularDetailsConfigurableSection() {
  return InternetDetailsSubpage() + "cr-expand-button#configurableSections";
}

WebContentsInteractionTestUtil::DeepQuery CellularDetailsProxySection() {
  return InternetDetailsSubpage() + "cr-expand-button#proxySectionToggle";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockToggle() {
  return InternetDetailsSubpage() + "network-siminfo#cellularSimInfoAdvanced" +
         "cr-toggle#simLockButton";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockTogglePolicyIcon() {
  return InternetDetailsSubpage() + "network-siminfo#cellularSimInfoAdvanced" +
         "cr-policy-indicator#simLockPolicyIcon";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockChangePinButton() {
  return InternetDetailsSubpage() + "network-siminfo#cellularSimInfoAdvanced" +
         "cr-button#changePinButton";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockDialogs() {
  return InternetDetailsSubpage() + "network-siminfo#cellularSimInfoAdvanced" +
         "sim-lock-dialogs";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockEnterPinDialogPolicySubtitle() {
  return CellularSimLockDialogs() + "div#adminSubtitle";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockEnterPinDialogButton() {
  return CellularSimLockDialogs() + "cr-button#enterPinButton";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockEnterPinDialogSubtext() {
  return CellularSimLockDialogs() + "div#pinEntrySubtext";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockEnterPinDialogPin() {
  return CellularSimLockDialogs() + "network-password-input#enterPin" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockChangePinDialogButton() {
  return CellularSimLockDialogs() + "cr-button#changePinButton";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockChangePinDialogNew() {
  return CellularSimLockDialogs() + "network-password-input#changePinNew1" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockChangePinDialogNewConfirm() {
  return CellularSimLockDialogs() + "network-password-input#changePinNew2" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockChangePinDialogOld() {
  return CellularSimLockDialogs() + "network-password-input#changePinOld" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockUnlockPinDialogButton() {
  return CellularSimLockDialogs() + "cr-button#unlockPinButton";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockUnlockPinDialogPin() {
  return CellularSimLockDialogs() + "network-password-input#unlockPin" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockUnlockPukDialogButton() {
  return CellularSimLockDialogs() + "cr-button#unlockPukButton";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockUnlockPukDialogPin() {
  return CellularSimLockDialogs() + "network-password-input#unlockPin1" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery CellularSimLockUnlockPukDialogPuk() {
  return CellularSimLockDialogs() + "network-password-input#unlockPuk" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSimLockUnlockPukDialogPinConfirm() {
  return CellularSimLockDialogs() + "network-password-input#unlockPin2" +
         "cr-input#input" + "input#input";
}

WebContentsInteractionTestUtil::DeepQuery CellularSubpageMenuRenameButton() {
  return ash::settings::InternetPage() + "settings-internet-detail-menu" +
         "button#renameBtn";
}

WebContentsInteractionTestUtil::DeepQuery CellularSubpageMenuRenameDialog() {
  return ash::settings::InternetPage() + "esim-rename-dialog#esimRenameDialog";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSubpageMenuRenameDialogDoneButton() {
  return CellularSubpageMenuRenameDialog() + "cr-button#done";
}

WebContentsInteractionTestUtil::DeepQuery
CellularSubpageMenuRenameDialogInputField() {
  return CellularSubpageMenuRenameDialog() + "cr-input#eSimprofileName";
}

WebContentsInteractionTestUtil::DeepQuery CellularSubpageApnRow() {
  return InternetDetailsSubpage() + "cr-link-row#apnSubpageButton";
}

}  // namespace cellular

namespace ethernet {

WebContentsInteractionTestUtil::DeepQuery EthernetSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#Ethernet" +
         "div#networkSummaryItemRow";
}

}  // namespace ethernet

namespace hotspot {

WebContentsInteractionTestUtil::DeepQuery HotspotSummaryItem() {
  return InternetPage() + "network-summary" + "hotspot-summary-item" +
         "div#hotspotSummaryItemRow";
}

WebContentsInteractionTestUtil::DeepQuery HotspotSummarySubtitleLink() {
  return InternetPage() + "network-summary" + "hotspot-summary-item" +
         "localized-link#hotspotDisabledSublabelLink" + "span";
}

WebContentsInteractionTestUtil::DeepQuery HotspotToggle() {
  return InternetPage() + "network-summary" + "hotspot-summary-item" +
         "cr-toggle#enableHotspotToggle";
}

WebContentsInteractionTestUtil::DeepQuery HotspotPolicyIcon() {
  return InternetPage() + "network-summary" + "hotspot-summary-item" +
         "div#hotspotSummaryItemRow" + "cr-policy-indicator#policyIndicator";
}

WebContentsInteractionTestUtil::DeepQuery HotspotConfigureButton() {
  return InternetPage() + "settings-hotspot-subpage" +
         "cr-button#configureButton";
}

WebContentsInteractionTestUtil::DeepQuery HotspotClientCountItem() {
  return InternetPage() + "settings-hotspot-subpage" +
         "div#connectedDeviceCountRow";
}

WebContentsInteractionTestUtil::DeepQuery HotspotConfigDialog() {
  return InternetPage() + "hotspot-config-dialog";
}

WebContentsInteractionTestUtil::DeepQuery HotspotDialogSaveButton() {
  return HotspotConfigDialog() + "cr-button#saveButton";
}

WebContentsInteractionTestUtil::DeepQuery HotspotSSID() {
  return InternetPage() + "settings-hotspot-subpage" + "div#hotspotSSID";
}

WebContentsInteractionTestUtil::DeepQuery HotspotSSIDInput() {
  return HotspotConfigDialog() + "network-config-input#hotspotName" +
         "cr-input" + "input#input";
}

}  // namespace hotspot

namespace wifi {

WebContentsInteractionTestUtil::DeepQuery WifiNetworksList() {
  return InternetPage() + "settings-internet-subpage" +
         "network-list#networkList";
}

WebContentsInteractionTestUtil::DeepQuery WifiSubpageEnableToggle() {
  return InternetPage() + "settings-internet-subpage" +
         "cr-toggle#deviceEnabledButton";
}

WebContentsInteractionTestUtil::DeepQuery WifiSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#WiFi" +
         "div#networkSummaryItemRow";
}

WebContentsInteractionTestUtil::DeepQuery AddWifiButton() {
  return InternetPage() + "settings-internet-subpage" +
         "cr-icon-button#addWifiButton";
}

WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialog() {
  return InternetPage() + "internet-config#configDialog" +
         "network-config#networkConfig";
}

WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialogSsidInput() {
  return ConfigureWifiDialog() + "network-config-input#ssid" + "cr-input" +
         "input#input";
}

WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialogShareToggle() {
  return ConfigureWifiDialog() + "network-config-toggle#share";
}

WebContentsInteractionTestUtil::DeepQuery ConfigureWifiDialogConnectButton() {
  return InternetPage() + "internet-config#configDialog" +
         "cr-button#connectButton";
}

WebContentsInteractionTestUtil::DeepQuery WifiKnownNetworksSubpageButton() {
  return InternetPage() + "settings-internet-subpage" +
         "cr-link-row#knownNetworksSubpageButton";
}

WebContentsInteractionTestUtil::DeepQuery KnownNetworksSubpage() {
  return InternetPage() + "settings-internet-known-networks-subpage";
}

WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointSubsciptions() {
  return KnownNetworksSubpage() + "div#passpointSubscriptionList";
}

WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointSubscriptionItem() {
  return KnownNetworksSubpage() + "cr-link-row#subscriptionItem" + "div#label";
}

WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointMoreButton() {
  return KnownNetworksSubpage() + "cr-icon-button#subscriptionMoreButton";
}

WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointDotsMenu() {
  return KnownNetworksSubpage() + "cr-action-menu#subscriptionDotsMenu";
}

WebContentsInteractionTestUtil::DeepQuery
KnownNetworksSubpagePasspointSubscriptionForget() {
  return KnownNetworksSubpage() + "button#subscriptionForget";
}

WebContentsInteractionTestUtil::DeepQuery PasspointSubpageExpirationDate() {
  return InternetPage() + "settings-passpoint-subpage" +
         "div#passpointExpirationDate";
}

WebContentsInteractionTestUtil::DeepQuery PasspointSubpageProviderSource() {
  return InternetPage() + "settings-passpoint-subpage" +
         "div#passpointSourceText";
}

WebContentsInteractionTestUtil::DeepQuery
PasspointSubpageAssociatedNetworksListItem() {
  return InternetPage() + "settings-passpoint-subpage" + "cr-link-row" +
         "div#label";
}

WebContentsInteractionTestUtil::DeepQuery
PasspointSubpageDomainExpansionButton() {
  return InternetPage() + "settings-passpoint-subpage" + "cr-expand-button";
}

WebContentsInteractionTestUtil::DeepQuery PasspointSubpageDomainList() {
  return InternetPage() + "settings-passpoint-subpage" + "iron-collapse";
}

WebContentsInteractionTestUtil::DeepQuery PasspointSubpageDomainListItem() {
  return InternetPage() + "settings-passpoint-subpage" + "div#domainName";
}

WebContentsInteractionTestUtil::DeepQuery PasspointSubpageRemoveButton() {
  return InternetPage() + "settings-passpoint-subpage" +
         "cr-button#removeButton";
}

WebContentsInteractionTestUtil::DeepQuery PasspointSubpageRemoveDialog() {
  return InternetPage() + "settings-passpoint-subpage" +
         "cr-dialog#removalDialog";
}

WebContentsInteractionTestUtil::DeepQuery
PasspointSubpageRemoveDialogConfirmButton() {
  return InternetPage() + "settings-passpoint-subpage" +
         "cr-button#removalConfirmButton";
}

}  // namespace wifi

namespace vpn {

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogServiceNameInput() {
  return InternetPage() + "internet-config#configDialog" +
         "network-config#networkConfig" + "network-config-input#vpn-name-input";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogProviderTypeSelect() {
  return InternetPage() + "internet-config#configDialog" +
         "network-config#networkConfig" +
         "network-config-select#vpn-type-select";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogProviderTypeOptions() {
  return InternetConfigDialog() + "network-config-select#vpn-type-select" +
         "div#inner" + "select";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogHostnameInput() {
  return InternetConfigDialog() + "network-config-input#vpn-host-input";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogOpenVpnUsernameInput() {
  return InternetConfigDialog() + "network-config-input#openvpn-username-input";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogOpenVpnPasswordInput() {
  return InternetConfigDialog() +
         "network-password-input#openvpn-password-input";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogOpenVpnOtpInput() {
  return InternetConfigDialog() + "network-config-input#openvpn-otp-input";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogSaveCredentialsToggle() {
  return InternetConfigDialog() +
         "network-config-toggle#vpn-save-credentials-toggle";
}

WebContentsInteractionTestUtil::DeepQuery JoinVpnDialogConnectButton() {
  return InternetPage() + "internet-config#configDialog" +
         "cr-button#connectButton";
}

WebContentsInteractionTestUtil::DeepQuery VpnSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#VPN" +
         "div#networkSummaryItemRow";
}

WebContentsInteractionTestUtil::DeepQuery VpnNetworksList() {
  return InternetPage() + "settings-internet-subpage" + "network-list";
}

WebContentsInteractionTestUtil::DeepQuery VpnNetworksListFirstItem() {
  return VpnNetworksList() + "network-list-item:first-of-type";
}

WebContentsInteractionTestUtil::DeepQuery VpnSubpageProviderType() {
  return InternetDetailsSubpage() + "network-property-list-mojo#infoFields" +
         "div#vpn\\.type";
}

WebContentsInteractionTestUtil::DeepQuery VpnSubpageHostnameInput() {
  return InternetDetailsSubpage() + "network-property-list-mojo#infoFields" +
         "cr-input#vpn\\.host" + "input";
}

WebContentsInteractionTestUtil::DeepQuery VpnSubpageUsernameInput() {
  return InternetDetailsSubpage() + "network-property-list-mojo#infoFields" +
         "cr-input#vpn\\.openVpn\\.username" + "input";
}

}  // namespace vpn

namespace bluetooth {

WebContentsInteractionTestUtil::DeepQuery BluetoothPage() {
  return WebContentsInteractionTestUtil::DeepQuery({{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "os-settings-bluetooth-page",
  }});
}

WebContentsInteractionTestUtil::DeepQuery BluetoothPairNewDeviceButton() {
  return BluetoothPage() + "cr-button#pairNewDevice";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothPairingDialog() {
  return BluetoothPage() + "os-settings-bluetooth-pairing-dialog";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothDeviceList() {
  return BluetoothPage() + "os-settings-bluetooth-devices-subpage" +
         "os-settings-paired-bluetooth-list";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothSubpageToggle() {
  return BluetoothPage() + "os-settings-bluetooth-devices-subpage" +
         "cr-toggle#enableBluetoothToggle";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothDeviceDetailSubpage() {
  return BluetoothPage() + "os-settings-bluetooth-device-detail-subpage";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothChangeDeviceNameButton() {
  return BluetoothDeviceDetailSubpage() + "cr-button#changeNameBtn";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothBatteryPercentage() {
  return BluetoothDeviceDetailSubpage() +
         "bluetooth-device-battery-info#batteryInfo" +
         "bluetooth-battery-icon-percentage#defaultBattery" +
         "span#batteryPercentage";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothForgetDeviceButton() {
  return BluetoothDeviceDetailSubpage() + "cr-button#forgetBtn";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothDeviceName() {
  return BluetoothDeviceDetailSubpage() + "div#bluetoothDeviceNameLabel";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothRenameDialog() {
  return BluetoothDeviceDetailSubpage() +
         "os-settings-bluetooth-change-device-name-dialog";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothRenameDialogInputField() {
  return BluetoothRenameDialog() + "cr-input#changeNameInput";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothRenameDialogDoneButton() {
  return BluetoothRenameDialog() + "cr-button#done";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothForgetDialog() {
  return BluetoothDeviceDetailSubpage() +
         "os-settings-bluetooth-forget-device-dialog";
}

WebContentsInteractionTestUtil::DeepQuery BluetoothForgetDialogDoneButton() {
  return BluetoothForgetDialog() + "cr-button#forget";
}

}  // namespace bluetooth

}  // namespace ash::settings
