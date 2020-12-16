// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/network_ui/network_health_localized_strings.h"

#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace network_health {

namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    // Network Health Summary Strings
    {"NetworkHealthState", IDS_NETWORK_HEALTH_STATE},
    {"NetworkHealthStateUninitialized", IDS_NETWORK_HEALTH_STATE_UNINITIALIZED},
    {"NetworkHealthStateDisabled", IDS_NETWORK_HEALTH_STATE_DISABLED},
    {"NetworkHealthStateProhibited", IDS_NETWORK_HEALTH_STATE_PROHIBITED},
    {"NetworkHealthStateNotConnected", IDS_NETWORK_HEALTH_STATE_NOT_CONNECTED},
    {"NetworkHealthStateConnecting", IDS_NETWORK_HEALTH_STATE_CONNECTING},
    {"NetworkHealthStatePortal", IDS_NETWORK_HEALTH_STATE_PORTAL},
    {"NetworkHealthStateConnected", IDS_NETWORK_HEALTH_STATE_CONNECTED},
    {"NetworkHealthStateOnline", IDS_NETWORK_HEALTH_STATE_ONLINE},

    {"OncType", IDS_NETWORK_TYPE},
    {"OncName", IDS_ONC_NAME},
    {"OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR},
    {"OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET},
    {"OncTypeMobile", IDS_NETWORK_TYPE_MOBILE_DATA},
    {"OncTypeTether", IDS_NETWORK_TYPE_TETHER},
    {"OncTypeVPN", IDS_NETWORK_TYPE_VPN},
    {"OncTypeWireless", IDS_NETWORK_TYPE_WIRELESS},
    {"OncTypeWiFi", IDS_NETWORK_TYPE_WIFI},
    {"OncWiFi-SignalStrength", IDS_ONC_WIFI_SIGNAL_STRENGTH},
    {"OncMacAddress", IDS_ONC_MAC_ADDRESS},
};

}  // namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  for (const auto& str : kLocalizedStrings)
    html_source->AddLocalizedString(str.name, str.id);
}

}  // namespace network_health
}  // namespace chromeos
