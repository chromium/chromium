// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/diagnostics_app.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {DiagnosticsAppElement} from 'chrome://diagnostics/diagnostics_app.js';
import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage, fakeKeyboards, fakeMemoryUsage, fakeSystemInfo, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {KeyboardInfo} from 'chrome://diagnostics/input.mojom-webui.js';
import {setInputDataProviderForTesting, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('appTestSuiteForInputHiding', function() {
  let page: DiagnosticsAppElement|null = null;

  const systemDataProvider = new FakeSystemDataProvider();

  const inputDataProvider = new FakeInputDataProvider();

  const DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();

  suiteSetup(() => {
    systemDataProvider.setFakeSystemInfo(fakeSystemInfo);
    systemDataProvider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);
    systemDataProvider.setFakeBatteryHealth(fakeBatteryHealth);
    systemDataProvider.setFakeBatteryInfo(fakeBatteryInfo);
    systemDataProvider.setFakeCpuUsage(fakeCpuUsage);
    systemDataProvider.setFakeMemoryUsage(fakeMemoryUsage);
    setSystemDataProviderForTesting(systemDataProvider);

    setInputDataProviderForTesting(inputDataProvider);

    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues(
        {isTouchpadEnabled: false, isTouchscreenEnabled: false});
  });

  teardown(() => {
    loadTimeData.overrideValues(
        {isTouchpadEnabled: true, isTouchscreenEnabled: true});

    page?.remove();
    page = null;
    inputDataProvider.reset();
  });

  function initializeDiagnosticsApp(keyboards: KeyboardInfo[]): Promise<void> {
    inputDataProvider.setFakeConnectedDevices(keyboards, fakeTouchDevices);

    page = document.createElement('diagnostics-app');
    assert(page);
    document.body.appendChild(page);
    return flushTasks();
  }

  function navigationSelectorHasId(id: string): boolean {
    assert(page);
    const items =
        page.shadowRoot!.querySelector('navigation-view-panel')!.shadowRoot!
            .querySelector('navigation-selector')!.selectorItems;
    return !!items.find((item: SelectorItem) => item.id === id);
  }

  test('InputPageHiddenWhenNoKeyboardsConnected', async () => {
    await initializeDiagnosticsApp([]);
    assertFalse(navigationSelectorHasId('input'));

    inputDataProvider.addFakeConnectedKeyboard(
        (fakeKeyboards[0] as KeyboardInfo));
    await flushTasks();
    assertTrue(navigationSelectorHasId('input'));

    inputDataProvider.removeFakeConnectedKeyboardById(fakeKeyboards[0]!.id);
    await flushTasks();
    assertFalse(navigationSelectorHasId('input'));
  });

  test('InputPageShownWhenKeyboardConnectedAtLaunch', async () => {
    await initializeDiagnosticsApp([(fakeKeyboards[0] as KeyboardInfo)]);
    assertTrue(navigationSelectorHasId('input'));

    inputDataProvider.removeFakeConnectedKeyboardById(fakeKeyboards[0]!.id);
    await flushTasks();
    assertFalse(navigationSelectorHasId('input'));
  });
});
