// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://diagnostics.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

[['Test', 'diagnostics/diagnostics_app_test.js'],
 ['CpuCard', 'diagnostics/cpu_card_test.js'],
 ['DataPoint', 'diagnostics/data_point_test.js'],
 ['OverviewCard', 'diagnostics/overview_card_test.js'],
 ['MemoryCard', 'diagnostics/memory_card_test.js'],
 ['BatteryStatusCard', 'diagnostics/battery_status_card_test.js'],
 ['FakeObservables', 'diagnostics/fake_observables_test.js'],
 ['FakeMojoInterface', 'diagnostics/mojo_interface_provider_test.js'],
 ['FakeSystemDataProvider', 'diagnostics/fake_system_data_provider_test.js'],
 ['FakeMethodProvider', 'diagnostics/fake_method_provider_test.js'],
 ['RealtimeCpuChart', 'diagnostics/realtime_cpu_chart_test.js'],
 ['RoutineListExecutor', 'diagnostics/routine_list_executor_test.js'],
 ['RoutineResultEntry', 'diagnostics/routine_result_entry_test.js'],
 ['RoutineResultList', 'diagnostics/routine_result_list_test.js'],
 ['RoutineSection', 'diagnostics/routine_section_test.js'],
 ['PercentBarChart', 'diagnostics/percent_bar_chart_test.js'], [
   'FakeSystemRoutineController',
   'diagnostics/fake_system_routine_controller_test.js'
 ]].forEach(test => registerTest(...test));

function registerTest(testName, module) {
  const className = `DiagnosticsApp${testName}`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      return `chrome://diagnostics/test_loader.html?module=chromeos/${module}`;
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
          'chromeos::features::kDiagnosticsApp',
        ],
      };
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
