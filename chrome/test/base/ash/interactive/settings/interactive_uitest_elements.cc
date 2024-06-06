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

namespace cellular {

WebContentsInteractionTestUtil::DeepQuery EsimDialogInstallingMessage() {
  return EsimDialog() + "setup-loading-page#profileInstallingPage" +
         "base-page" + "div#message";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogFirstProfile() {
  return EsimDialog() + "profile-discovery-list-page" +
         "profile-discovery-list-item:first-of-type";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogForwardButton() {
  return InternetPage() + "os-settings-cellular-setup-dialog" +
         "cellular-setup" + "button-bar" + "cr-button#forward";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialogTitle() {
  return InternetPage() + "os-settings-cellular-setup-dialog" + "div#header";
}

WebContentsInteractionTestUtil::DeepQuery EsimDialog() {
  return InternetPage() + "os-settings-cellular-setup-dialog" +
         "cellular-setup" + "esim-flow-ui";
}

WebContentsInteractionTestUtil::DeepQuery AddEsimButton() {
  return InternetPage() + "settings-internet-subpage" +
         "cellular-networks-list" + "cr-icon-button#addESimButton";
}

WebContentsInteractionTestUtil::DeepQuery CellularSummaryItem() {
  return InternetPage() + "network-summary" + "network-summary-item#Cellular" +
         "div#networkSummaryItemRow";
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
