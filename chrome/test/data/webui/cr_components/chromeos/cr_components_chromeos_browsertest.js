// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

// Polymer 2 test list format:
//
// ['ModuleNameTest', 'module.js',
//   [<module.js dependency source list>]
// ]
// clang-format off
[
  ['CrPolicyNetworkBehaviorMojo', 'network/cr_policy_network_behavior_mojo_tests.js',
    ['../../cr_elements/cr_policy_strings.js']
  ],
  ['CrPolicyNetworkIndicatorMojo', 'network/cr_policy_network_indicator_mojo_tests.js',
    ['../../cr_elements/cr_policy_strings.js']
  ],
  ['NetworkApnlist', 'network/network_apnlist_test.js', []],
  ['NetworkChooseMobile', 'network/network_choose_mobile_test.js', []],
  ['NetworkConfig', 'network/network_config_test.js',
    [
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/promise_resolver.js',
      '../../fake_chrome_event.js',
      '../../chromeos/networking_private_constants.js',
      '../../chromeos/fake_network_config_mojom.js',
    ]
  ],
  ['NetworkConfigElementBehavior', 'network/network_config_element_behavior_test.js', []],
  ['NetworkConfigInput', 'network/network_config_input_test.js', []],
  ['NetworkConfigSelect', 'network/network_config_select_test.js', []],
  ['NetworkConfigToggle', 'network/network_config_toggle_test.js', []],
  ['NetworkIpConfig', 'network/network_ip_config_test.js', []],
  ['NetworkList', 'network/network_list_test.js', []],
  ['NetworkListItem', 'network/network_list_item_test.js', []],
  ['NetworkNameservers', 'network/network_nameservers_test.js', []],
  ['NetworkPasswordInput', 'network/network_password_input_test.js', []],
  ['NetworkPropertyListMojo', 'network/network_property_list_mojo_test.js', []],
  ['NetworkProxyExclusions', 'network/network_proxy_exclusions_test.js', []],
  ['NetworkProxyInput', 'network/network_proxy_input_test.js', []],
  ['NetworkProxy', 'network/network_proxy_test.js', []],
  ['NetworkSiminfo', 'network/network_siminfo_test.js', []],
].forEach(test => registerTest('NetworkComponents', 'os-settings', ...test));

[
  ['NetworkSelect', 'network/network_select_test.js', []],
].forEach(test => registerTest('NetworkComponents', 'network', ...test));

[
  ['BasePage', 'cellular_setup/base_page_test.js', []],
  ['ButtonBar', 'cellular_setup/button_bar_test.js',[]],
  ['CellularSetup', 'cellular_setup/cellular_setup_test.js', [
    './cellular_setup/fake_cellular_setup_delegate.js',
  ]],
  ['EsimFlowUi', 'cellular_setup/esim_flow_ui_test.js',[
    './cellular_setup/fake_cellular_setup_delegate.js',
  ]],
  ['FinalPage', 'cellular_setup/final_page_test.js', [
    './cellular_setup/fake_cellular_setup_delegate.js',
  ]],
  ['ProvisioningPage', 'cellular_setup/provisioning_page_test.js',[
    './cellular_setup/fake_cellular_setup_delegate.js',
  ]],
  ['PsimFlowUi', 'cellular_setup/psim_flow_ui_test.js',[
    './cellular_setup/fake_cellular_setup_delegate.js',
  ]],
  ['SetupSelectionFlow', 'cellular_setup/setup_selection_flow_test.js',[
    './cellular_setup/fake_cellular_setup_delegate.js',
  ]],
  ['SimDetectPage', 'cellular_setup/sim_detect_page_test.js', [
    './cellular_setup/fake_cellular_setup_delegate.js',
  ]],
].forEach(test => registerTest('CellularSetup', 'cellular-setup', ...test));
// clang-format on

function registerTest(componentName, webuiHost, testName, module, deps) {
  const className = `${componentName}${testName}Test`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      return `chrome://${webuiHost}/`;
    }

    /** @override */
    get extraLibraries() {
      return super.extraLibraries.concat(module).concat(deps);
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
