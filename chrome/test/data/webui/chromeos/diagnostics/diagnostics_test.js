// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/diagnostics_app.js';

import {SystemDataProviderInterface} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeSystemInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeMethodResolver} from 'chrome://diagnostics/fake_method_resolver.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('DiagnosticsFakeMethodResolver', () => {
  /** @type {?FakeMethodResolver} */
  let resolver = null;

  setup(function() {
    resolver = new FakeMethodResolver();
  });

  teardown(function() {
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

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('diagnostics-app');
    assertTrue(!!page);
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  test('LandingPageLoaded', () => {
    // TODO(zentaro): Update when strings are finalized and localized.
    assertEquals('Diagnostics', page.$$('#header').textContent);

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

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (batteryStatusElement) {
      batteryStatusElement.remove();
    }
    batteryStatusElement = null;
    provider = null;
  });

  function initializeBatteryStatusCard() {
    assertFalse(!!batteryStatusElement);

    // Add the battery status card to the DOM.
    batteryStatusElement = document.createElement('battery-status-card');
    assertTrue(!!batteryStatusElement);
    document.body.appendChild(batteryStatusElement);

    return flushTasks();
  }

  test('BatterStatusCardPopulated', () => {
    return initializeBatteryStatusCard().then(() => {
      // TODO(zentaro): Update when strings are finalized.
      assertEquals(
          'Battery Status', batteryStatusElement.$$('#cardTitle').textContent);
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

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (cpuElement) {
      cpuElement.remove();
    }
    cpuElement = null;
    provider = null;
  });

  function initializeCpuCard() {
    assertFalse(!!cpuElement);

    // Add the CPU card to the DOM.
    cpuElement = document.createElement('cpu-card');
    assertTrue(!!cpuElement);
    document.body.appendChild(cpuElement);

    return flushTasks();
  }

  test('CpuCardPopulated', () => {
    return initializeCpuCard().then(() => {
      // TODO(zentaro): Update when strings are finalized.
      assertEquals('CPU', cpuElement.$$('#cardTitle').textContent);
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

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    overviewElement.remove();
    overviewElement = null;
    provider = null;
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

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (memoryElement) {
      memoryElement.remove();
    }
    memoryElement = null;
    provider = null;
  });

  function initializeMemoryCard() {
    assertFalse(!!memoryElement);

    // Add the memory card to the DOM.
    memoryElement = document.createElement('memory-card');
    assertTrue(!!memoryElement);
    document.body.appendChild(memoryElement);

    return flushTasks();
  }

  test('MemoryCardPopulated', () => {
    return initializeMemoryCard().then(() => {
      // TODO(zentaro): Update when strings are finalized.
      assertEquals('Memory', memoryElement.$$('#cardTitle').textContent);
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

suite('FakeSystemDataProviderTest', () => {
  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  setup(function() {
    provider = new FakeSystemDataProvider();
  });

  teardown(function() {
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
});
