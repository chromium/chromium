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
 * To run each test in a new instance:
 * `browser_tests --run-manual --gtest_filter=DiagnosticsApp.MANUAL_*``
 *
 * To run a single test suite, such as 'CpuCard':
 * `browser_tests --run-manual --gtest_filter=DiagnosticsApp.MANUAL_CpuCard`
 *
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const dxTestSuites = 'chromeos/diagnostics/diagnostics_app_unified_test.js';
const diagnosticsUrl =
    `chrome://diagnostics/test_loader.html?module=${dxTestSuites}&host=test`;

this.DiagnosticsApp = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return diagnosticsUrl;
  }

  /** @override */
  get featureList() {}
};

this.DiagnosticsAppWithNetwork = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return diagnosticsUrl;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kEnableNetworkingInDiagnosticsApp',
      ],
    };
  }
};

this.DiagnosticsAppWithInput = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return diagnosticsUrl;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kEnableInputInDiagnosticsApp',
        'chromeos::features::kEnableTouchpadsInDiagnosticsApp',
        'chromeos::features::kEnableTouchscreensInDiagnosticsApp',
      ],
    };
  }
};

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'App',
  'AppForInputHiding',
  'BatteryStatusCard',
  'CellularInfo',
  'ConnectivityCard',
  'CpuCard',
  'DataPoint',
  'DiagnosticsNetworkIcon',
  'DiagnosticsStickyBanner',
  'DiagnosticsUtils',
  'DrawingProvider',
  'EthernetInfo',
  'FakeMojoInterface',
  'FakeNetworkHealthProvider',
  'FakeSystemDataProvider',
  'FakeSystemRoutineContoller',
  'FrequencyChannelUtils',
  'InputCard',
  'InputList',
  'IpConfigInfoDrawer',
  'KeyboardTester',
  'MemoryCard',
  'NetworkCard',
  'NetworkInfo',
  'NetworkList',
  'NetworkTroubleshooting',
  'OverviewCard',
  'PercentBarChart',
  'RealtimeCpuChart',
  'RoutineGroup',
  'RoutineListExecutor',
  'RoutineResultEntry',
  'RoutineResultList',
  'RoutineSection',
  'SystemPage',
  'TextBadge',
  'TouchscreenTester',
  'WifiInfo',
];

// Flaky: https://crbug.com/1372958
TEST_F('DiagnosticsApp', 'DISABLED_BrowserTest', function() {
  assertDeepEquals(
      debug_suites_list, Object.keys(test_suites_list),
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');

  mocha.run();
});

// Flaky: https://crbug.com/1372958
TEST_F('DiagnosticsAppWithNetwork', 'DISABLED_BrowserTest', function() {
  mocha.run();
});

// Flaky: https://crbug.com/1372958
TEST_F('DiagnosticsAppWithInput', 'DISABLED_BrowserTest', function() {
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F('DiagnosticsApp', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });

  TEST_F('DiagnosticsAppWithNetwork', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });

  TEST_F('DiagnosticsAppWithInput', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });
}
