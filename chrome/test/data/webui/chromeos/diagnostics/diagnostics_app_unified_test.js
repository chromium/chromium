// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {batteryStatusCardTestSuite} from './battery_status_card_test.js';
import {cellularInfoTestSuite} from './cellular_info_test.js';
import {connectivityCardTestSuite} from './connectivity_card_test.js';
import {cpuCardTestSuite} from './cpu_card_test.js';
import {dataPointTestSuite} from './data_point_test.js';
import {appTestSuite} from './diagnostics_app_test.js';
import {diagnosticsUtilsTestSuite} from './diagnostics_utils_test.js';
import {ethernetInfoTestSuite} from './ethernet_info_test.js';
import {fakeNetworkHealthProviderTestSuite} from './fake_network_health_provider_test.js';
import {fakeSystemDataProviderTestSuite} from './fake_system_data_provider_test.js';
import {fakeSystemRoutineContollerTestSuite} from './fake_system_routine_controller_test.js';
import {memoryCardTestSuite} from './memory_card_test.js';
import {fakeMojoProviderTestSuite} from './mojo_interface_provider_test.js';
import {networkCardTestSuite} from './network_card_test.js';
import {networkInfoTestSuite} from './network_info_test.js';
import {networkListTestSuite} from './network_list_test.js';
import {overviewCardTestSuite} from './overview_card_test.js';
import {percentBarChartTestSuite} from './percent_bar_chart_test.js';
import {realtimeCpuChartTestSuite} from './realtime_cpu_chart_test.js';
import {fakeRoutineListExecutorTestSuite} from './routine_list_executor_test.js';
import {routineResultEntryTestSuite} from './routine_result_entry_test.js';
import {routineResultListTestSuite} from './routine_result_list_test.js';
import {routineSectionTestSuite} from './routine_section_test.js';
import {textBadgeTestSuite} from './text_badge_test.js';
import {wifiInfoTestSuite} from './wifi_info_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('App', appTestSuite);
runSuite('BatteryStatusCard', batteryStatusCardTestSuite);
runSuite('CellularInfo', cellularInfoTestSuite);
runSuite('ConnectivityCard', connectivityCardTestSuite);
runSuite('CpuCard', cpuCardTestSuite);
runSuite('DataPoint', dataPointTestSuite);
runSuite('DiagnosticsUtils', diagnosticsUtilsTestSuite);
runSuite('EthernetInfo', ethernetInfoTestSuite);
runSuite('FakeMojoInterface', fakeMojoProviderTestSuite);
runSuite('FakeNetworkHealthProvider', fakeNetworkHealthProviderTestSuite);
runSuite('FakeSystemDataProvider', fakeSystemDataProviderTestSuite);
runSuite('FakeSystemRoutineContoller', fakeSystemRoutineContollerTestSuite);
runSuite('MemoryCard', memoryCardTestSuite);
runSuite('NetworkCard', networkCardTestSuite);
runSuite('NetworkInfo', networkInfoTestSuite);
runSuite('NetworkList', networkListTestSuite);
runSuite('OverviewCard', overviewCardTestSuite);
runSuite('PercentBarChart', percentBarChartTestSuite);
runSuite('RealtimeCpuChart', realtimeCpuChartTestSuite);
runSuite('RoutineListExecutor', fakeRoutineListExecutorTestSuite);
runSuite('RoutineResultEntry', routineResultEntryTestSuite);
runSuite('RoutineResultList', routineResultListTestSuite);
runSuite('RoutineSection', routineSectionTestSuite);
runSuite('TextBadge', textBadgeTestSuite);
runSuite('WifiInfo', wifiInfoTestSuite);
