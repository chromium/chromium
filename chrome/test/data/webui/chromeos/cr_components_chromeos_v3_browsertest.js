// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 cr_components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

// clang-format off
[
  ['BasePage', 'bluetooth/bluetooth_base_page_test.js'],
  ['BluetoothIcon', 'bluetooth/bluetooth_icon_test.js'],
  [
    'BatteryIconPercentage',
    'bluetooth/bluetooth_battery_icon_percentage_test.js',
  ],
  ['DeviceBatteryInfo', 'bluetooth/bluetooth_device_battery_info_test.js'],
  [
    'DeviceSelectionPage',
    'bluetooth/bluetooth_pairing_device_selection_page_test.js',
  ],
  [
    'PairingConfirmCode',
    'bluetooth/bluetooth_pairing_confirm_code_page_test.js',
  ],
  ['PairingDeviceItem', 'bluetooth/bluetooth_pairing_device_item_test.js'],
  [
    'PairingRequestCodePage',
    'bluetooth/bluetooth_pairing_request_code_page_test.js',
  ],
  [
    'PairingEnterCodePage',
    'bluetooth/bluetooth_pairing_enter_code_page_test.js',
  ],
  ['PairingUi', 'bluetooth/bluetooth_pairing_ui_test.js'],
  ['SpinnerPage', 'bluetooth/bluetooth_spinner_page_test.js'],
 ].forEach(test => registerWebUiTest('Bluetooth', 'bluetooth-pairing', ...test));

[['ApnList', 'network/apn_list_test.js'],
 ['ApnListItem', 'network/apn_list_item_test.js'],
 ['ApnSelectionDialog', 'network/apn_selection_dialog_test.js'],
 ['ApnSelectionDialogListItem', 'network/apn_selection_dialog_list_item_test.js'],
 ['CrPolicyNetworkBehaviorMojo', 'network/cr_policy_network_behavior_mojo_tests.js'],
 ['CrPolicyNetworkIndicatorMojo', 'network/cr_policy_network_indicator_mojo_tests.js'],
 ['NetworkApnlist', 'network/network_apnlist_test.js'],
 ['NetworkChooseMobile', 'network/network_choose_mobile_test.js'],
 ['NetworkConfig', 'network/network_config_test.js'],
 ['NetworkConfigElementBehavior', 'network/network_config_element_behavior_test.js'],
 ['NetworkConfigInput', 'network/network_config_input_test.js'],
 ['NetworkConfigSelect', 'network/network_config_select_test.js'],
 ['NetworkConfigToggle', 'network/network_config_toggle_test.js'],
 ['NetworkConfigVpnTest', 'network/network_config_vpn_test.js'],
 ['NetworkConfigWifi', 'network/network_config_wifi_test.js'],
 ['NetworkIcon', 'network/network_icon_test.js'],
 ['NetworkIpConfig', 'network/network_ip_config_test.js'],
 ['NetworkList', 'network/network_list_test.js'],
 ['NetworkListItem', 'network/network_list_item_test.js'],
 ['NetworkNameservers', 'network/network_nameservers_test.js'],
 ['NetworkPasswordInput', 'network/network_password_input_test.js'],
 ['NetworkPropertyListMojo', 'network/network_property_list_mojo_test.js'],
 ['NetworkProxyExclusions', 'network/network_proxy_exclusions_test.js'],
 ['NetworkProxyInput', 'network/network_proxy_input_test.js'],
 ['NetworkProxy', 'network/network_proxy_test.js'],
 ['NetworkSelect', 'network/network_select_test.js'],
 ['NetworkSiminfo', 'network/network_siminfo_test.js'],
 ['SimLockDialogs', 'network/sim_lock_dialogs_test.js'],
].forEach(test => registerWebUiTest('NetworkComponents', 'os-settings', ...test));

[['NetworkDiagnostics', 'network_health/network_diagnostics_test.js'],
 ['RoutineGroup', 'network_health/routine_group_test.js'],
].forEach(test => registerWebUiTest('NetworkHealth', 'connectivity-diagnostics', ...test));

[['TrafficCounters', 'traffic_counters/traffic_counters_test.js'],
].forEach(test => registerWebUiTest('TrafficCounters', 'network', ...test));

[
 ['Integration', 'multidevice_setup/integration_test.js'],
 ['SetupSucceededPage', 'multidevice_setup/setup_succeeded_page_test.js'],
 ['StartSetupPage', 'multidevice_setup/start_setup_page_test.js'],
].forEach(test => registerWebUiTest('MultiDeviceSetup', 'multidevice-setup', ...test));

[
 ['ActivationCodePage', 'cellular_setup/activation_code_page_test.js'],
 ['BasePage', 'cellular_setup/base_page_test.js'],
 ['ButtonBar', 'cellular_setup/button_bar_test.js'],
 ['CellularSetup', 'cellular_setup/cellular_setup_test.js'],
 ['ConfirmationCodePage', 'cellular_setup/confirmation_code_page_test.js'],
 ['ProfileDiscoveryListPage', 'cellular_setup/profile_discovery_list_page_test.js'],
 ['EsimFlowUi', 'cellular_setup/esim_flow_ui_test.js'],
 ['FinalPage', 'cellular_setup/final_page_test.js'],
 ['ProvisioningPage', 'cellular_setup/provisioning_page_test.js'],
 ['PsimFlowUi', 'cellular_setup/psim_flow_ui_test.js'],
 ['SetupLoadingPage', 'cellular_setup/setup_loading_page_test.js'],
].forEach(test => registerWebUiTest('CellularSetup', 'os-settings', ...test));
// clang-format on

function registerWebUiTest(componentName, webuiHost, testName, module) {
  const className = `${componentName}${testName}TestV3`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      // TODO(jhawkins): Set up test_loader.html for internet-config-dialog
      // and use it here instead of os-settings.
      return `chrome://${webuiHost}/test_loader.html?module=chromeos/${module}`;
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
