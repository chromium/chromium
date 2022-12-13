// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/diagnostics_app.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {DiagnosticsAppElement} from 'chrome://diagnostics/diagnostics_app.js';
import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage, fakeKeyboards, fakeMemoryUsage, fakeSystemInfo, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {KeyboardInfo} from 'chrome://diagnostics/input_data_provider.mojom-webui.js';
import {setInputDataProviderForTesting, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('appTestSuiteForInputHiding', function() {
  /** @type {?DiagnosticsAppElement} */
  let page = null;

  /** @type {?FakeSystemDataProvider} */
  let systemDataProvider = null;

  /** @type {?FakeInputDataProvider} */
  let inputDataProvider = null;

  /** @type {?TestDiagnosticsBrowserProxy} */
  let DiagnosticsBrowserProxy = null;

  suiteSetup(() => {
    systemDataProvider = new FakeSystemDataProvider();
    systemDataProvider.setFakeSystemInfo(fakeSystemInfo);
    systemDataProvider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);
    systemDataProvider.setFakeBatteryHealth(fakeBatteryHealth);
    systemDataProvider.setFakeBatteryInfo(fakeBatteryInfo);
    systemDataProvider.setFakeCpuUsage(fakeCpuUsage);
    systemDataProvider.setFakeMemoryUsage(fakeMemoryUsage);
    setSystemDataProviderForTesting(systemDataProvider);

    inputDataProvider = new FakeInputDataProvider();
    setInputDataProviderForTesting(inputDataProvider);

    DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);
  });

  setup(() => {
    document.body.innerHTML = '';

    loadTimeData.overrideValues(
        {isTouchpadEnabled: false, isTouchscreenEnabled: false});
  });

  teardown(() => {
    loadTimeData.overrideValues(
        {isTouchpadEnabled: true, isTouchscreenEnabled: true});

    page.remove();
    page = null;
    inputDataProvider.reset();
  });

  /** @param {!Array<!KeyboardInfo>} keyboards */
  function initializeDiagnosticsApp(keyboards) {
    assertFalse(!!page);

    inputDataProvider.setFakeConnectedDevices(keyboards, fakeTouchDevices);

    page = /** @type {!DiagnosticsAppElement} */ (
        document.createElement('diagnostics-app'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /** @param {!string} id */
  function navigationSelectorHasId(id) {
    const items = page.shadowRoot.querySelector('navigation-view-panel')
                      .shadowRoot.querySelector('navigation-selector')
                      .selectorItems;
    return !!items.find((item) => item.id === id);
  }

  test('InputPageHiddenWhenNoKeyboardsConnected', async () => {
    await initializeDiagnosticsApp([]);
    assertFalse(navigationSelectorHasId('input'));

    inputDataProvider.addFakeConnectedKeyboard(fakeKeyboards[0]);
    await flushTasks();
    assertTrue(navigationSelectorHasId('input'));

    inputDataProvider.removeFakeConnectedKeyboardById(fakeKeyboards[0].id);
    await flushTasks();
    assertFalse(navigationSelectorHasId('input'));
  });

  test('InputPageShownWhenKeyboardConnectedAtLaunch', async () => {
    await initializeDiagnosticsApp([fakeKeyboards[0]]);
    assertTrue(navigationSelectorHasId('input'));

    inputDataProvider.removeFakeConnectedKeyboardById(fakeKeyboards[0].id);
    await flushTasks();
    assertFalse(navigationSelectorHasId('input'));
  });
});
