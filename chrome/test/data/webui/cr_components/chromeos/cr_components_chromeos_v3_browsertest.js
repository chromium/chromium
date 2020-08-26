// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 cr_components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

// clang-format off
[['CrPolicyNetworkBehaviorMojo', 'network/cr_policy_network_behavior_mojo_tests.m.js'],
 ['CrPolicyNetworkIndicatorMojo', 'network/cr_policy_network_indicator_mojo_tests.m.js'],
 ['NetworkApnlist', 'network/network_apnlist_test.m.js'],
 ['NetworkConfigElementBehavior', 'network/network_config_element_behavior_test.m.js'],
 ['NetworkIpConfig', 'network/network_ip_config_test.m.js'],
 ['NetworkList', 'network/network_list_test.m.js'],
 ['NetworkListItem', 'network/network_list_item_test.m.js'],
 ['NetworkNameservers', 'network/network_nameservers_test.m.js'],
 ['NetworkPasswordInput', 'network/network_password_input_test.m.js'],
 ['NetworkPropertyListMojo', 'network/network_property_list_mojo_test.m.js'],
].forEach(test => registerTest('NetworkComponents', ...test));

[['BasePage', 'cellular_setup/base_page_test.m.js'],
 ['ButtonBar', 'cellular_setup/button_bar_test.m.js'],
 ['CellularSetup', 'cellular_setup/cellular_setup_test.m.js'],
 ['EsimFlowUi', 'cellular_setup/esim_flow_ui_test.m.js'],
 ['FinalPage', 'cellular_setup/final_page_test.m.js'],
 ['ProvisioningPage', 'cellular_setup/provisioning_page_test.m.js'],
 ['PsimFlowUi', 'cellular_setup/psim_flow_ui_test.m.js'],
 ['SetupSelectionFlow', 'cellular_setup/setup_selection_flow_test.m.js'],
 ['SimDetectPage', 'cellular_setup/sim_detect_page_test.m.js'],
].forEach(test => registerTest('CellularSetup', ...test));
// clang-format on

function registerTest(componentName, testName, module, caseName) {
  const className = `${componentName}${testName}TestV3`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      // TODO(jhawkins): Set up test_loader.html for internet-config-dialog
      // and use it here instead of os-settings.
      return `chrome://os-settings/test_loader.html?module=cr_components/chromeos/${module}`;
    }

    /** @override */
    get extraLibraries() {
      return [
        '//third_party/mocha/mocha.js',
        '//chrome/test/data/webui/mocha_adapter.js',
      ];
    }

    /** @override */
    get featureList() {
      return {
        enabled: [
          'chromeos::features::kOsSettingsPolymer3',
          'chromeos::features::kUpdatedCellularActivationUi',
        ],
      };
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
