// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {batteryStatusCardTestSuite} from './battery_status_card_test.js';
import {cpuCardTestSuite} from './cpu_card_test.js';
import {dataPointTestSuite} from './data_point_test.js';
import {appTestSuite} from './diagnostics_app_test.js';
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

suite('App', appTestSuite);
suite('BatteryStatusCard', batteryStatusCardTestSuite);
suite('CpuCard', cpuCardTestSuite);
suite('DataPoint', dataPointTestSuite);
suite('FakeMethodProvider', fakeMethodResolverTestSuite);
suite('FakeMojoInterface', fakeMojoProviderTestSuite);
suite('FakeObservables', fakeObservablesTestSuite);
suite('FakeSystemDataProvider', fakeSystemDataProviderTestSuite);
suite('FakeSystemRoutineContoller', fakeSystemRoutineContollerTestSuite);
suite('MemoryCard', memoryCardTestSuite);
suite('OverviewCard', overviewCardTestSuite);
suite('PercentBarChart', percentBarChartTestSuite);
suite('RealtimeCpuChart', realtimeCpuChartTestSuite);
suite('RoutineListExecutor', fakeRoutineListExecutorTestSuite);
suite('RoutineResultEntry', routineResultEntryTestSuite);
suite('RoutineResultList', routineResultListTestSuite);
suite('RoutineSection', routineSectionTestSuite);
