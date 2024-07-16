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

WebContentsInteractionTestUtil::DeepQuery InternetDetailsSubpage() {
  return InternetPage() + "settings-internet-detail-subpage";
}

WebContentsInteractionTestUtil::DeepQuery NetworkMoreDetailsMenuButton() {
  return InternetPage() + "settings-internet-detail-menu" +
         "cr-icon-button#moreNetworkDetail";
}

WebContentsInteractionTestUtil::DeepQuery SettingsSubpageTitle() {
  return InternetPage() + "os-settings-subpage" + "h1#subpageTitle";
}

namespace cellular {

WebContentsInteractionTestUtil::DeepQuery ApnDialog() {
  return InternetPage() + "apn-subpage" + "apn-list" + "apn-detail-dialog";
}

WebContentsInteractionTestUtil::DeepQuery ApnDialogAdvancedSettingsButton() {
  return ApnDialog() + "cr-expand-button";
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
  return InternetPage() + "apn-subpage" + "apn-list" +
         "apn-list-item:first-of-type" + "div#apnName";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpageActionMenuButton() {
  return InternetPage() + "cr-icon-button#apnActionMenuButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpageCreateApnButton() {
  return InternetPage() + "button#createCustomApnButton";
}

WebContentsInteractionTestUtil::DeepQuery ApnSubpageZeroStateContent() {
  return InternetPage() + "apn-subpage" + "apn-list" + "div#zeroStateContent";
}

WebContentsInteractionTestUtil::DeepQuery CellularSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#Cellular" +
         "div#networkSummaryItemRow";
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

WebContentsInteractionTestUtil::DeepQuery CellularSubpagePsimListTitle() {
  return CellularNetworksList() + "div#pSimLabel";
}

WebContentsInteractionTestUtil::DeepQuery CellularDetailsSubpageTitle() {
  return InternetPage() + "os-settings-subpage" + "h1#subpageTitle";
}

WebContentsInteractionTestUtil::DeepQuery
CellularDetailsSubpageAutoConnectToggle() {
  return InternetDetailsSubpage() + "settings-toggle-button#autoConnectToggle";
}

WebContentsInteractionTestUtil::DeepQuery
CellularDetailsAllowDataRoamingToggle() {
  return InternetDetailsSubpage() + "cellular-roaming-toggle-button";
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
  return InternetPage() + "settings-internet-detail-subpage" +
         "cr-link-row#apnSubpageButton";
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

WebContentsInteractionTestUtil::DeepQuery WifiSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#WiFi" +
         "div#networkSummaryItemRow";
}

}  // namespace wifi

namespace vpn {

WebContentsInteractionTestUtil::DeepQuery VpnSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#VPN" +
         "div#networkSummaryItemRow";
}

}  // namespace vpn

}  // namespace ash::settings
