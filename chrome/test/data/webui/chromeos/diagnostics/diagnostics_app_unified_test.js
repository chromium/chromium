// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {batteryStatusCardTestSuite} from './battery_status_card_test.js';
import {connectivityCardTestSuite} from './connectivity_card_test.js';
import {cpuCardTestSuite} from './cpu_card_test.js';
import {dataPointTestSuite} from './data_point_test.js';
import {appTestSuite} from './diagnostics_app_test.js';
import {diagnosticsUtilsTestSuite} from './diagnostics_utils_test.js';
import {fakeMethodResolverTestSuite} from './fake_method_provider_test.js';
import {fakeObservablesTestSuite} from './fake_observables_test.js';
import {fakeSystemDataProviderTestSuite} from './fake_system_data_provider_test.js';
import {fakeSystemRoutineContollerTestSuite} from './fake_system_routine_controller_test.js';
import {memoryCardTestSuite} from './memory_card_test.js';
import {fakeMojoProviderTestSuite} from './mojo_interface_provider_test.js';
import {overviewCardTestSuite} from './overview_card_test.js';
import {percentBarChartTestSuite} from './percent_bar_chart_test.js';
import {realtimeCpuChartTestSuite} from './realtime_cpu_chart_test.js';
import {fakeRoutineListExecutorTestSuite} from './routine_list_executor_test.js';
import {routineResultEntryTestSuite} from './routine_result_entry_test.js';
import {routineResultListTestSuite} from './routine_result_list_test.js';
import {routineSectionTestSuite} from './routine_section_test.js';
import {textBadgeTestSuite} from './text_badge_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('App', appTestSuite);
runSuite('BatteryStatusCard', batteryStatusCardTestSuite);
runSuite('ConnectivityCard', connectivityCardTestSuite);
runSuite('CpuCard', cpuCardTestSuite);
runSuite('DataPoint', dataPointTestSuite);
runSuite('DiagnosticsUtils', diagnosticsUtilsTestSuite);
runSuite('FakeMethodProvider', fakeMethodResolverTestSuite);
runSuite('FakeMojoInterface', fakeMojoProviderTestSuite);
runSuite('FakeObservables', fakeObservablesTestSuite);
runSuite('FakeSystemDataProvider', fakeSystemDataProviderTestSuite);
runSuite('FakeSystemRoutineContoller', fakeSystemRoutineContollerTestSuite);
runSuite('MemoryCard', memoryCardTestSuite);
runSuite('OverviewCard', overviewCardTestSuite);
runSuite('PercentBarChart', percentBarChartTestSuite);
runSuite('RealtimeCpuChart', realtimeCpuChartTestSuite);
runSuite('RoutineListExecutor', fakeRoutineListExecutorTestSuite);
runSuite('RoutineResultEntry', routineResultEntryTestSuite);
runSuite('RoutineResultList', routineResultListTestSuite);
runSuite('RoutineSection', routineSectionTestSuite);
runSuite('TextBadge', textBadgeTestSuite);
