// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 cr_components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

// clang-format off
[['CrPolicyNetworkBehaviorMojo', 'network/cr_policy_network_behavior_mojo_tests.m.js'],
 ['CrPolicyNetworkIndicatorMojo', 'network/cr_policy_network_indicator_mojo_tests.m.js'],
 ['NetworkApnlist', 'network/network_apnlist_test.m.js'],
 ['NetworkChooseMobile', 'network/network_choose_mobile_test.m.js'],
 ['NetworkConfig', 'network/network_config_test.m.js'],
 ['NetworkConfigElementBehavior', 'network/network_config_element_behavior_test.m.js'],
 ['NetworkConfigInput', 'network/network_config_input_test.m.js'],
 ['NetworkConfigSelect', 'network/network_config_select_test.m.js'],
 ['NetworkConfigToggle', 'network/network_config_toggle_test.m.js'],
 ['NetworkIpConfig', 'network/network_ip_config_test.m.js'],
 ['NetworkList', 'network/network_list_test.m.js'],
 ['NetworkListItem', 'network/network_list_item_test.m.js'],
 ['NetworkNameservers', 'network/network_nameservers_test.m.js'],
 ['NetworkPasswordInput', 'network/network_password_input_test.m.js'],
 ['NetworkPropertyListMojo', 'network/network_property_list_mojo_test.m.js'],
 ['NetworkProxyExclusions', 'network/network_proxy_exclusions_test.m.js'],
 ['NetworkProxyInput', 'network/network_proxy_input_test.m.js'],
 ['NetworkProxy', 'network/network_proxy_test.m.js'],
 ['NetworkSelect', 'network/network_select_test.m.js'],
 ['NetworkSiminfo', 'network/network_siminfo_test.m.js'],
 ['SimLockDialogs', 'network/sim_lock_dialogs_test.m.js'],
].forEach(test => registerTest('NetworkComponents', 'os-settings', ...test));

[['RoutineGroup', 'network_health/routine_group_test.m.js'],
].forEach(test => registerTest('NetworkHealth', 'connectivity-diagnostics', ...test));

[
 ['Integration', 'multidevice_setup/integration_test.m.js'],
 ['SetupSucceededPage', 'multidevice_setup/setup_succeeded_page_test.m.js'],
 ['StartSetupPage', 'multidevice_setup/start_setup_page_test.m.js'],
].forEach(test => registerTest('MultiDeviceSetup', 'multidevice-setup', ...test));

[
 ['ActivationCodePage', 'cellular_setup/activation_code_page_test.m.js'],
 ['BasePage', 'cellular_setup/base_page_test.m.js'],
 ['ButtonBar', 'cellular_setup/button_bar_test.m.js'],
 ['ConfirmationCodePage', 'cellular_setup/confirmation_code_page_test.m.js'],
 ['CellularEidPopup', 'cellular_setup/cellular_eid_popup_test.m.js'],
 ['CellularSetup', 'cellular_setup/cellular_setup_test.m.js'],
 ['EsimFlowUi', 'cellular_setup/esim_flow_ui_test.m.js'],
 ['FinalPage', 'cellular_setup/final_page_test.m.js'],
 ['ProvisioningPage', 'cellular_setup/provisioning_page_test.m.js'],
 ['PsimFlowUi', 'cellular_setup/psim_flow_ui_test.m.js'],
 ['SetupLoadingPage', 'cellular_setup/setup_loading_page_test.m.js'],
].forEach(test => registerTest('CellularSetup', 'os-settings', ...test));
// clang-format on

function registerTest(componentName, webuiHost, testName, module, caseName) {
  const className = `${componentName}${testName}TestV3`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      // TODO(jhawkins): Set up test_loader.html for internet-config-dialog
      // and use it here instead of os-settings.
      return `chrome://${
          webuiHost}/test_loader.html?module=cr_components/chromeos/${module}`;
    }

    /** @override */
    get featureList() {
      return {
        enabled: [
          'chromeos::features::kUpdatedCellularActivationUi',
          'chromeos::features::kCellularUseAttachApn',
        ],
      };
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
