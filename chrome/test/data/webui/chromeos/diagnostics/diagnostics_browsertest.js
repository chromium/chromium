// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test fixture for chrome://diagnostics.
 * Unifieid polymer testing suite for diagnostics app.
 *
 * To run all tests in a single instance (default, faster):
 * `browser_tests --gtest_filter=DiagnosticsApp*``
 *
 * To run a single test suite, such as 'CpuCard':
 * `browser_tests
 * --gtest_filter=DiagnosticsApp_CpuCard.All`
 *
 * To run a single test suite, such as 'TouchscreenTester':
 * `browser_tests
 * --gtest_filter=DiagnosticsAppWithInput_TouchscreenTester.All`
 *
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const DiagnosticsApp = class extends PolymerTest {
  /** @override */
  get featureList() {}
};

const DiagnosticsAppWithInput = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kEnableTouchpadsInDiagnosticsApp',
        'ash::features::kEnableTouchscreensInDiagnosticsApp',
      ],
    };
  }
};

const tests = [
  ['App', 'diagnostics_app_test.js'],
  ['AppForInputHiding', 'diagnostics_app_input_hiding_test.js', 'Input'],
  ['BatteryStatusCard', 'battery_status_card_test.js'],
  ['CellularInfo', 'cellular_info_test.js'],
  ['ConnectivityCard', 'connectivity_card_test.js'],
  ['CpuCard', 'cpu_card_test.js'],
  ['DataPoint', 'data_point_test.js'],
  ['DiagnosticsNetworkIcon', 'diagnostics_network_icon_test.js'],
  ['DiagnosticsStickyBanner', 'diagnostics_sticky_banner_test.js'],
  ['DiagnosticsUtils', 'diagnostics_utils_test.js'],
  ['DrawingProvider', 'drawing_provider_test.js'],
  ['DrawingProviderUtils', 'drawing_provider_utils_test.js'],
  ['EthernetInfo', 'ethernet_info_test.js'],
  ['FakeMojoInterface', 'mojo_interface_provider_test.js'],
  ['FakeNetworkHealthProvider', 'fake_network_health_provider_test.js'],
  ['FakeSystemDataProvider', 'fake_system_data_provider_test.js'],
  ['FakeSystemRoutineContoller', 'fake_system_routine_controller_test.js'],
  ['FrequencyChannelUtils', 'frequency_channel_utils_test.js'],
  ['InputCard', 'input_card_test.js', 'Input'],
  ['InputList', 'input_list_test.js', 'Input'],
  ['IpConfigInfoDrawer', 'ip_config_info_drawer_test.js'],
  ['KeyboardTester', 'keyboard_tester_test.js', 'Input'],
  ['MemoryCard', 'memory_card_test.js'],
  ['NetworkCard', 'network_card_test.js'],
  ['NetworkInfo', 'network_info_test.js'],
  ['NetworkList', 'network_list_test.js'],
  ['NetworkTroubleshooting', 'network_troubleshooting_test.js'],
  ['OverviewCard', 'overview_card_test.js'],
  ['PercentBarChart', 'percent_bar_chart_test.js'],
  ['RealtimeCpuChart', 'realtime_cpu_chart_test.js'],
  ['RoutineGroup', 'routine_group_test.js'],
  ['RoutineListExecutor', 'routine_list_executor_test.js'],
  ['RoutineResultEntry', 'routine_result_entry_test.js'],
  ['RoutineResultList', 'routine_result_list_test.js'],
  ['RoutineSection', 'routine_section_test.js'],
  ['SystemPage', 'system_page_test.js'],
  ['TextBadge', 'text_badge_test.js'],
  ['TouchscreenTester', 'touchscreen_tester_test.js', 'Input'],
  ['TouchpadTester', 'touchpad_tester_test.js', 'Input'],
  ['WifiInfo', 'wifi_info_test.js'],
];

tests.forEach(([testName, module, condition, caseName]) => {
  const className =
      `DiagnosticsApp${condition ? `With${condition}` : ''}_${testName}`;

  let classToExtend = DiagnosticsApp;
  if (condition === 'Input') {
    classToExtend = DiagnosticsAppWithInput;
  }

  this[className] = class extends classToExtend {
    /** @override */
    get browsePreload() {
      return `chrome://diagnostics/test_loader.html` +
          `?module=chromeos/diagnostics/${module}`;
    }
  }

  TEST_F(className, caseName || 'All', () => mocha.run());
});
