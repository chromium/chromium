// Copyright 2020 The Chromium Authors. All rights reserved.
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

this['DiagnosticsApp'] = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return `chrome://diagnostics/test_loader.html?module=${dxTestSuites}`;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kDiagnosticsApp',
        'chromeos::features::kEnableNetworkingInDiagnosticsApp',
      ],
    };
  }
};

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'App',
  'BatteryStatusCard',
  'ConnectivityCard',
  'CpuCard',
  'DataPoint',
  'DiagnosticsUtils',
  'FakeMethodProvider',
  'FakeMojoInterface',
  'FakeObservables',
  'FakeSystemDataProvider',
  'FakeSystemRoutineContoller',
  'MemoryCard',
  'OverviewCard',
  'PercentBarChart',
  'RealtimeCpuChart',
  'RoutineListExecutor',
  'RoutineResultEntry',
  'RoutineResultList',
  'RoutineSection',
  'TextBadge'
];

TEST_F('DiagnosticsApp', 'BrowserTest', function() {
  assertDeepEquals(
      debug_suites_list, test_suites_list,
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F('DiagnosticsApp', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });
}
