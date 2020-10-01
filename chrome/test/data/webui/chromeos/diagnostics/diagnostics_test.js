// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/battery_status_card.js';
import 'chrome://diagnostics/cpu_card.js';
import 'chrome://diagnostics/data_point.js';
import 'chrome://diagnostics/diagnostics_app.js';
import 'chrome://diagnostics/memory_card.js';
import 'chrome://diagnostics/overview_card.js';

import {SystemDataProviderInterface} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeBatteryInfo2, fakeCpuUsage, fakeMemoryUsage, fakeSystemInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeMethodResolver} from 'chrome://diagnostics/fake_method_resolver.js';
import {FakeObservables} from 'chrome://diagnostics/fake_observables.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.m.js';
import * as diagnostics_test_utils from './diagnostics_test_utils.js';

suite('DiagnosticsFakeMethodResolver', () => {
  /** @type {?FakeMethodResolver} */
  let resolver = null;

  setup(() => {
    resolver = new FakeMethodResolver();
  });

  teardown(() => {
    resolver = null;
  });

  test('AddingMethodNoResult', () => {
    resolver.register('foo');
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(undefined, result);
    });
  });

  test('AddingMethodWithResult', () => {
    resolver.register('foo');
    const expected = {'foo': 'bar'};
    resolver.setResult('foo', expected);
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(expected, result);
    });
  });

  test('AddingTwoMethodCallingOne', () => {
    resolver.register('foo');
    resolver.register('bar');
    const expected = {'fooKey': 'fooValue'};
    resolver.setResult('foo', expected);
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(expected, result);
    });
  });
});

suite('DiagnosticsAppTest', () => {
  /** @type {?DiagnosticsApp} */
  let page = null;

  setup(() => {
    PolymerTest.clearBody();
    page = document.createElement('diagnostics-app');
    assertTrue(!!page);
    document.body.appendChild(page);
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  test('LandingPageLoaded', () => {
    // Verify the overview card is in the page.
    const overview = page.$$('#overviewCard');
    assertTrue(!!overview);

    // Verify the memory card is in the page.
    const memory = page.$$('#memoryCard');
    assertTrue(!!memory);

    // Verify the CPU card is in the page.
    const cpu = page.$$('#cpuCard');
    assertTrue(!!cpu);

    // Verify the battery status card is in the page.
    const batteryStatus = page.$$('#batteryStatusCard');
    assertTrue(!!batteryStatus);

    // Verify the session log button is in the page.
    const sessionLog = page.$$('.session-log-button');
    assertTrue(!!sessionLog);
  });
});

suite('BatteryStatusCardTest', () => {
  /** @type {?HTMLElement} */
  let batteryStatusElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (batteryStatusElement) {
      batteryStatusElement.remove();
    }
    batteryStatusElement = null;
    provider.reset();
  });

  /**
   * @param {!BatteryInfo} batteryInfo
   * @param {!BatteryChargeStatus} batteryChargeStatus
   * @param {!BatteryHealth} batteryHealth
   * @return {!Promise}
   */
  function initializeBatteryStatusCard(
      batteryInfo, batteryChargeStatus, batteryHealth) {
    assertFalse(!!batteryStatusElement);

    // Initialize the fake data.
    provider.setFakeBatteryChargeStatus(batteryChargeStatus);
    provider.setFakeBatteryHealth(batteryHealth);
    provider.setFakeBatteryInfo(batteryInfo);

    // Add the battery status card to the DOM.
    batteryStatusElement = document.createElement('battery-status-card');
    assertTrue(!!batteryStatusElement);
    document.body.appendChild(batteryStatusElement);

    return flushTasks();
  }

  test('BatteryStatusCardPopulated', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          const dataPoints =
            diagnostics_test_utils.getDataPointElements(batteryStatusElement);
          assertEquals(
              fakeBatteryChargeStatus[0].current_now_milliamps,
              dataPoints[0].value);
          assertEquals(
              fakeBatteryHealth[0].charge_full_design_milliamp_hours,
              dataPoints[1].value);
          assertEquals(
              fakeBatteryChargeStatus[0]
                  .charge_full_now_milliamp_hours,
                  dataPoints[2].value);
          assertEquals(
              fakeBatteryChargeStatus[0].charge_now_milliamp_hours,
              dataPoints[3].value);
          assertEquals(
              fakeBatteryChargeStatus[0].power_time,
              dataPoints[4].value);
          assertEquals(
              fakeBatteryChargeStatus[0].power_adapter_status,
              dataPoints[5].value);
          assertEquals(fakeBatteryHealth[0].cycle_count, dataPoints[6].value);

          const barChart = diagnostics_test_utils.getPercentBarChartElement(
              batteryStatusElement);
          assertEquals(
              fakeBatteryChargeStatus[0].charge_full_now_milliamp_hours,
              barChart.max);
          assertEquals(
              fakeBatteryChargeStatus[0].charge_now_milliamp_hours,
              barChart.value);
        });
  });
});

suite('CpuCardTest', () => {
  /** @type {?HTMLElement} */
  let cpuElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (cpuElement) {
      cpuElement.remove();
    }
    cpuElement = null;
    provider.reset();
  });

  /**
   * @param {!CpuUsage} cpuUsage
   * @return {!Promise}
   */
  function initializeCpuCard(cpuUsage) {
    assertFalse(!!cpuElement);

    // Initialize the fake data.
    provider.setFakeCpuUsage(cpuUsage);

    // Add the CPU card to the DOM.
    cpuElement = document.createElement('cpu-card');
    assertTrue(!!cpuElement);
    document.body.appendChild(cpuElement);

    return flushTasks();
  }

  test('CpuCardPopulated', () => {
    return initializeCpuCard(fakeCpuUsage).then(() => {
      const dataPoints =
          diagnostics_test_utils.getDataPointElements(cpuElement);

      assertEquals(
          fakeCpuUsage[0].cpu_temp_degrees_celcius, dataPoints[0].value);
      assertEquals(fakeCpuUsage[0].percent_usage_user, dataPoints[1].value);
      assertEquals(fakeCpuUsage[0].percent_usage_system, dataPoints[2].value);
    });
  });
});

suite('OverviewCardTest', () => {
  /** @type {?HTMLElement} */
  let overviewElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    overviewElement.remove();
    overviewElement = null;
    provider.reset();
  });

  /** @param {!SystemInfo} */
  function initializeOverviewCard(fakeSystemInfo) {
    assertFalse(!!overviewElement);

    // Initialize the fake data.
    provider.setFakeSystemInfo(fakeSystemInfo);

    // Add the overview card to the DOM.
    overviewElement = document.createElement('overview-card');
    assertTrue(!!overviewElement);
    document.body.appendChild(overviewElement);

    return flushTasks();
  }

  test('OverviewCardPopulated', () => {
    return initializeOverviewCard(fakeSystemInfo).then(() => {
      assertEquals(
          fakeSystemInfo.board_name,
          overviewElement.$$('#boardName').textContent);
      assertEquals(
          fakeSystemInfo.cpu_model_name,
          overviewElement.$$('#cpuModelName').textContent);
      assertEquals(
          fakeSystemInfo.total_memory_kib.toString(),
          overviewElement.$$('#totalMemory').textContent);
      assertEquals(
          fakeSystemInfo.version.milestone_version,
          overviewElement.$$('#version').textContent);
    });
  });
});

suite('MemoryCardTest', () => {
  /** @type {?HTMLElement} */
  let memoryElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (memoryElement) {
      memoryElement.remove();
    }
    memoryElement = null;
    provider.reset();
  });

  /**
   * @param {!MemoryUsage} memoryUsage
   * @return {!Promise}
   */
  function initializeMemoryCard(memoryUsage) {
    assertFalse(!!memoryElement);

    // Initialize the fake data.
    provider.setFakeMemoryUsage(memoryUsage);

    // Add the memory card to the DOM.
    memoryElement = document.createElement('memory-card');
    assertTrue(!!memoryElement);
    document.body.appendChild(memoryElement);

    return flushTasks();
  }

  test('MemoryCardPopulated', () => {
    return initializeMemoryCard(fakeMemoryUsage).then(() => {
      const dataPoints =
          diagnostics_test_utils.getDataPointElements(memoryElement);
      assertEquals(fakeMemoryUsage[0].total_memory_kib, dataPoints[0].value);
      assertEquals(
          fakeMemoryUsage[0].available_memory_kib, dataPoints[1].value);
      assertEquals(fakeMemoryUsage[0].free_memory_kib, dataPoints[2].value);

      const barChart =
          diagnostics_test_utils.getPercentBarChartElement(memoryElement);
      const memInUse = fakeMemoryUsage[0].total_memory_kib -
          fakeMemoryUsage[0].available_memory_kib;
      assertEquals(fakeMemoryUsage[0].total_memory_kib, barChart.max);
      assertEquals(memInUse, barChart.value);
    });
  });
});

suite('FakeMojoProviderTest', () => {
  test('SettingGettingTestProvider', () => {
    // TODO(zentaro): Replace with fake when built.
    let fake_provider =
        /** @type {SystemDataProviderInterface} */ (new Object());
    setSystemDataProviderForTesting(fake_provider);
    assertEquals(fake_provider, getSystemDataProvider());
  });
});

suite('FakeObservablesTest', () => {
  /** @type {?FakeObservables} */
  let observables = null;

  setup(() => {
    observables = new FakeObservables();
  });

  teardown(() => {
    observables = null;
  });

  test('RegisterSimpleObservable', () => {
    observables.register('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar'];
    observables.setObservableData('ObserveFoo_OnFooUpdated', expected);

    let resolver = new PromiseResolver();
    observables.observe('ObserveFoo_OnFooUpdated', (foo) => {
      assertEquals(expected[0], foo);
      resolver.resolve();
    });

    observables.trigger('ObserveFoo_OnFooUpdated');
    return resolver.promise;
  });

  test('TwoResults', () => {
    observables.register('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar1', 'bar2'];
    observables.setObservableData('ObserveFoo_OnFooUpdated', expected);

    // The first call will get 'bar1', and the second 'bar2'.
    let resolver = new PromiseResolver();
    const expectedCallCount = 2;
    let callCount = 0;
    observables.observe('ObserveFoo_OnFooUpdated', (foo) => {
      assertEquals(expected[callCount % expected.length], foo);
      callCount++;
      if (callCount === expectedCallCount) {
        resolver.resolve();
      }
    });

    // Trigger the observer twice.
    observables.trigger('ObserveFoo_OnFooUpdated');
    observables.trigger('ObserveFoo_OnFooUpdated');
    return resolver.promise;
  });

  test('ObservableDataWraps', () => {
    observables.register('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar1', 'bar2'];
    observables.setObservableData('ObserveFoo_OnFooUpdated', expected);

    // With 3 calls and 2 observable values the response should cycle
    // 'bar1', 'bar2', 'bar1'
    let resolver = new PromiseResolver();
    const expectedCallCount = 3;
    let callCount = 0;
    observables.observe('ObserveFoo_OnFooUpdated', (foo) => {
      assertEquals(expected[callCount % expected.length], foo);
      callCount++;
      if (callCount === expectedCallCount) {
        resolver.resolve();
      }
    });

    // Trigger the observer three times.
    observables.trigger('ObserveFoo_OnFooUpdated');
    observables.trigger('ObserveFoo_OnFooUpdated');
    observables.trigger('ObserveFoo_OnFooUpdated');
    return resolver.promise;
  });
});


suite('FakeSystemDataProviderTest', () => {
  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  setup(() => {
    provider = new FakeSystemDataProvider();
  });

  teardown(() => {
    provider = null;
  });

  test('GetSystemInfo', () => {
    /** @type {!DeviceCapabilities} */
    const capabilities = {
      has_battery: true,
    };

    /** @type {!VersionInfo} */
    const version = {
      milestone_version: 'M97',
    };

    /** @type {!SystemInfo} */
    const expected = {
      board_name: 'BestBoard',
      cpu_model: 'SuperFast CPU',
      total_memory_kib: 9999,
      cores_number: 4,
      version_info: version,
      device_capabilities: capabilities,
    };

    provider.setFakeSystemInfo(expected);
    return provider.getSystemInfo().then((systemInfo) => {
      assertDeepEquals(expected, systemInfo);
    });
  });

  test('GetBatteryInfo', () => {
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo().then((batteryInfo) => {
      assertDeepEquals(fakeBatteryInfo, batteryInfo);
    });
  });

  test('ObserveBatteryHealth', () => {
    provider.setFakeBatteryHealth(fakeBatteryHealth);

    /** @type {!BatteryHealthObserver} */
    const batteryHealthObserverRemote = {
      onBatteryHealthUpdated: (batteryHealth) => {
        assertDeepEquals(fakeBatteryHealth[0], batteryHealth);
      }
    };

    return provider.observeBatteryHealth(batteryHealthObserverRemote);
  });

  test('ObserveBatteryChargeStatus', () => {
    provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);

    /** @type {!BatteryChargeStatusObserver} */
    const batteryChargeStatusObserverRemote = {
      onBatteryChargeStatusUpdated: (batteryChargeStatus) => {
        assertDeepEquals(fakeBatteryChargeStatus[0], batteryChargeStatus);
      }
    };

    return provider.observeBatteryChargeStatus(
        batteryChargeStatusObserverRemote);
  });

  test('ObserveCpuUsage', () => {
    provider.setFakeCpuUsage(fakeCpuUsage);

    /** @type {!CpuUsageObserver} */
    const cpuObserverRemote = {
      onCpuUsageUpdated: (cpuUsage) => {
        assertDeepEquals(fakeCpuUsage[0], cpuUsage);
      }
    };

    return provider.observeCpuUsage(cpuObserverRemote);
  });

  test('ObserveMemoryUsage', () => {
    provider.setFakeMemoryUsage(fakeMemoryUsage);

    /** @type {!MemoryUsageObserver} */
    const memoryUsageObserverRemote = {
      onMemoryUsageUpdated: (memoryUsage) => {
        assertDeepEquals(fakeMemoryUsage[0], memoryUsage);
      }
    };

    return provider.observeMemoryUsage(memoryUsageObserverRemote);
  });

  test('CallMethodWithNoValue', () => {
    // Don't set any fake data.
    return provider.getBatteryInfo().then((value) => {
      assertEquals(undefined, value);
    });
  });

  test('CallMethodDifferentDataAfterReset', () => {
    // Setup the fake data, but then reset it and set it with new data.
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    provider.reset();
    provider.setFakeBatteryInfo(fakeBatteryInfo2);

    return provider.getBatteryInfo().then((batteryInfo) => {
      assertDeepEquals(fakeBatteryInfo2, batteryInfo);
    });
  });

  test('CallMethodFirstThenDifferentDataAfterReset', () => {
    // Setup the initial fake data.
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo()
        .then((batteryInfo) => {
          assertDeepEquals(fakeBatteryInfo, batteryInfo);
        })
        .then(() => {
          // Reset and next time it should fire undefined.
          provider.reset();
          return provider.getBatteryInfo().then((value) => {
            assertEquals(undefined, value);
          });
        })
        .then(() => {
          // Set different data and next time should fire with it.
          provider.reset();
          provider.setFakeBatteryInfo(fakeBatteryInfo2);
          return provider.getBatteryInfo().then((batteryInfo) => {
            assertDeepEquals(fakeBatteryInfo2, batteryInfo);
          });
        });
  });
});

suite('DataPointTest', () => {
  /** @type {?HTMLElement} */
  let dataPointElement = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (dataPointElement) {
      dataPointElement.remove();
    }
    dataPointElement = null;
  });

  /**
   * @param {string} title
   * @param {string} value
   */
  function initializeDataPoint(title, value) {
    assertFalse(!!dataPointElement);

    // Add the data point to the DOM.
    dataPointElement = document.createElement('data-point');
    assertTrue(!!dataPointElement);
    document.body.appendChild(dataPointElement);
    dataPointElement.title = title;
    dataPointElement.value = value;
    return flushTasks();
  }

  test('InitializeDataPoint', () => {
    const title = 'Test title';
    const value = 'Test value';
    return initializeDataPoint(title, value).then(() => {
      assertEquals(title, dataPointElement.$$('.title').textContent.trim());
      assertEquals(value, dataPointElement.$$('.value').textContent.trim());
    });
  });
});
