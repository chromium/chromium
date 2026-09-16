// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

// Bluetooth components
class CrComponentsCrOsBluetoothTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCrOsBluetoothTest() {
    set_test_loader_host(ash::kChromeUIBluetoothPairingHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest, BluetoothBasePage) {
  RunTest("chromeos/bluetooth/bluetooth_base_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest, BluetoothIcon) {
  RunTest("chromeos/bluetooth/bluetooth_icon_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest,
                       BluetoothBatteryIconPercentage) {
  RunTest("chromeos/bluetooth/bluetooth_battery_icon_percentage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest,
                       BluetoothDeviceBatteryInfo) {
  RunTest("chromeos/bluetooth/bluetooth_device_battery_info_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest,
                       BluetoothPairingDeviceSelectionPage) {
  RunTest("chromeos/bluetooth/bluetooth_pairing_device_selection_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest,
                       BluetoothPairingConfirmCodePage) {
  RunTest("chromeos/bluetooth/bluetooth_pairing_confirm_code_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest,
                       BluetoothPairingDeviceItem) {
  RunTest("chromeos/bluetooth/bluetooth_pairing_device_item_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest,
                       BluetoothPairingRequestCodePage) {
  RunTest("chromeos/bluetooth/bluetooth_pairing_request_code_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest,
                       BluetoothPairingEnterCodePage) {
  RunTest("chromeos/bluetooth/bluetooth_pairing_enter_code_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest, BluetoothPairingUi) {
  RunTest("chromeos/bluetooth/bluetooth_pairing_ui_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrOsBluetoothTest, BluetoothSpinnerPage) {
  RunTest("chromeos/bluetooth/bluetooth_spinner_page_test.js", "mocha.run()");
}

// NetworkComponents
class CrComponentsCrosNetworkTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCrosNetworkTest() {
    set_test_loader_host(ash::kChromeUIOSSettingsHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, ApnList) {
  RunTest("chromeos/network/apn_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, ApnListItem) {
  RunTest("chromeos/network/apn_list_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, ApnSelectionDialog) {
  RunTest("chromeos/network/apn_selection_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest,
                       ApnSelectionDialogListItem) {
  RunTest("chromeos/network/apn_selection_dialog_list_item_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest,
                       CrPolicyNetworkBehaviorMojo) {
  RunTest("chromeos/network/cr_policy_network_behavior_mojo_tests.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest,
                       CrPolicyNetworkIndicatorMojo) {
  RunTest("chromeos/network/cr_policy_network_indicator_mojo_tests.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkApnlist) {
  RunTest("chromeos/network/network_apnlist_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkChooseMobile) {
  RunTest("chromeos/network/network_choose_mobile_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkConfig) {
  RunTest("chromeos/network/network_config_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest,
                       NetworkConfigElementBehavior) {
  RunTest("chromeos/network/network_config_element_behavior_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkConfigInput) {
  RunTest("chromeos/network/network_config_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkConfigSelect) {
  RunTest("chromeos/network/network_config_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkConfigToggle) {
  RunTest("chromeos/network/network_config_toggle_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkConfigVpn) {
  RunTest("chromeos/network/network_config_vpn_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkConfigWifi) {
  RunTest("chromeos/network/network_config_wifi_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkIcon) {
  RunTest("chromeos/network/network_icon_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkIpConfig) {
  RunTest("chromeos/network/network_ip_config_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkList) {
  RunTest("chromeos/network/network_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkListItem) {
  RunTest("chromeos/network/network_list_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkNameservers) {
  RunTest("chromeos/network/network_nameservers_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkPasswordInput) {
  RunTest("chromeos/network/network_password_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkPropertyListMojo) {
  RunTest("chromeos/network/network_property_list_mojo_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkProxyExclusions) {
  RunTest("chromeos/network/network_proxy_exclusions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkProxyInput) {
  RunTest("chromeos/network/network_proxy_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkProxy) {
  RunTest("chromeos/network/network_proxy_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkSelect) {
  RunTest("chromeos/network/network_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, NetworkSiminfo) {
  RunTest("chromeos/network/network_siminfo_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkTest, SimLockDialogs) {
  RunTest("chromeos/network/sim_lock_dialogs_test.js", "mocha.run()");
}

// NetworkHealth
class CrComponentsCrosNetworkHealthTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCrosNetworkHealthTest() {
    set_test_loader_host(ash::kChromeUIConnectivityDiagnosticsHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkHealthTest, NetworkDiagnostics) {
  RunTest("chromeos/network_health/network_diagnostics_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosNetworkHealthTest, RoutineGroup) {
  RunTest("chromeos/network_health/routine_group_test.js", "mocha.run()");
}

// MultiDeviceSetup
class CrComponentsCrosMultiDeviceSetupTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCrosMultiDeviceSetupTest() {
    set_test_loader_host(ash::kChromeUIMultiDeviceSetupHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsCrosMultiDeviceSetupTest, Integration) {
  RunTest("chromeos/multidevice_setup/integration_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosMultiDeviceSetupTest,
                       SetupSucceededPage) {
  RunTest("chromeos/multidevice_setup/setup_succeeded_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosMultiDeviceSetupTest, StartSetupPage) {
  RunTest("chromeos/multidevice_setup/start_setup_page_test.js", "mocha.run()");
}

// CellularSetup
class CrComponentsCrosCellularSetupTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCrosCellularSetupTest() {
    set_test_loader_host(ash::kChromeUIOSSettingsHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, ActivationCodePage) {
  RunTest("chromeos/cellular_setup/activation_code_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, BasePage) {
  RunTest("chromeos/cellular_setup/base_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, ButtonBar) {
  RunTest("chromeos/cellular_setup/button_bar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, CellularSetup) {
  RunTest("chromeos/cellular_setup/cellular_setup_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest,
                       ConfirmationCodePage) {
  RunTest("chromeos/cellular_setup/confirmation_code_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest,
                       ProfileDiscoveryListPage) {
  RunTest("chromeos/cellular_setup/profile_discovery_list_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, EsimFlowUi) {
  RunTest("chromeos/cellular_setup/esim_flow_ui_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, FinalPage) {
  RunTest("chromeos/cellular_setup/final_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, ProvisioningPage) {
  RunTest("chromeos/cellular_setup/provisioning_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, PsimFlowUi) {
  RunTest("chromeos/cellular_setup/psim_flow_ui_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCrosCellularSetupTest, SetupLoadingPage) {
  RunTest("chromeos/cellular_setup/setup_loading_page_test.js", "mocha.run()");
}
