// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {BruschettaSubpageElement, CrostiniBrowserProxyImpl, CrostiniPortSetting} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

suite('<settings-bruschetta-subpage>', () => {
  let subpage: BruschettaSubpageElement;
  let crostiniBrowserProxy: TestCrostiniBrowserProxy;

  function setBruschettaPrefs(enabled: boolean, {
    sharedPaths = {},
    forwardedPorts = [],
    micAllowed = false,
    arcEnabled = false,
    bruschettaInstalled = false,
  }: PrefParams = {}): void {
    subpage.prefs = {
      arc: {
        enabled: {value: arcEnabled},
      },
      bruschetta: {
        installed: {
          value: bruschettaInstalled,
        },
        mic_allowed: {value: micAllowed},
      },
      crostini: {
        enabled: {value: enabled},
        mic_allowed: {value: micAllowed},
        port_forwarding: {ports: {value: forwardedPorts}},
      },
      guest_os: {
        paths_shared_to_vms: {value: sharedPaths},
      },
    };
    flush();
  }

  setup(async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
      showBruschetta: true,
    });
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);

    Router.getInstance().navigateTo(routes.BRUSCHETTA_DETAILS);

    clearBody();
    subpage = document.createElement('settings-bruschetta-subpage');
    document.body.appendChild(subpage);
    setBruschettaPrefs(false, {bruschettaInstalled: true});
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  const MIC_ALLOWED_PREF_PATH = 'prefs.bruschetta.mic_allowed.value';

  test('Basic mic permission', () => {
    assertTrue(isVisible(subpage.shadowRoot!.querySelector(
        '#bruschetta-mic-permission-toggle')));
  });

  test('Toggle bruschetta mic permission shutdown', async () => {
    // Bruschetta is assumed to be running when the page is loaded.
    assertTrue(crostiniBrowserProxy.bruschettaIsRunning);
    let toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#bruschetta-mic-permission-toggle');
    assertTrue(!!toggle);
    let dialog =
        subpage.shadowRoot!.querySelector('#bruschetta-mic-permission-dialog');
    assertNull(dialog);

    setBruschettaPrefs(true, {micAllowed: false, bruschettaInstalled: true});
    assertFalse(toggle.checked);
    assertFalse(subpage.get(MIC_ALLOWED_PREF_PATH));

    toggle.click();
    await flushTasks();

    dialog =
        subpage.shadowRoot!.querySelector('#bruschetta-mic-permission-dialog');
    assertTrue(!!dialog);
    const dialogClosedPromise = eventToPromise('close', dialog);
    const actionBtn =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!actionBtn);
    actionBtn.click();
    await Promise.all([dialogClosedPromise, flushTasks()]);
    assertEquals(1, crostiniBrowserProxy.getCallCount('shutdownBruschetta'));
    assertFalse(crostiniBrowserProxy.bruschettaIsRunning);
    assertNull(
        subpage.shadowRoot!.querySelector('#bruschetta-mic-permission-dialog'));
    toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#bruschetta-mic-permission-toggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);
    assertTrue(subpage.get(MIC_ALLOWED_PREF_PATH));

    // Bruschetta is now shutdown, this means that it doesn't need to be
    // restarted in order for changes to take effect, therefore no dialog is
    // needed and the mic sharing settings can be changed immediately.
    toggle.click();
    await flushTasks();
    assertNull(
        subpage.shadowRoot!.querySelector('#bruschetta-mic-permission-dialog'));
    assertFalse(toggle.checked);
    assertFalse(subpage.get(MIC_ALLOWED_PREF_PATH));
  });

  test('Mic Toggle permission cancel', async () => {
    // Bruschetta is assumed to be running when the page is loaded.
    let toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#bruschetta-mic-permission-toggle');
    assertTrue(!!toggle);
    let dialog =
        subpage.shadowRoot!.querySelector('#bruschetta-mic-permission-dialog');
    assertNull(dialog);

    setBruschettaPrefs(true, {micAllowed: true, bruschettaInstalled: true});
    assertTrue(toggle.checked);
    assertTrue(subpage.get(MIC_ALLOWED_PREF_PATH));

    toggle.click();
    await flushTasks();

    dialog =
        subpage.shadowRoot!.querySelector('#bruschetta-mic-permission-dialog');
    assertTrue(!!dialog);
    const dialogClosedPromise = eventToPromise('close', dialog);
    const cancelBtn =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
    assertTrue(!!cancelBtn);
    cancelBtn.click();
    await Promise.all([dialogClosedPromise, flushTasks()]);

    // Because the dialog was cancelled, the toggle should not have changed.
    assertNull(
        subpage.shadowRoot!.querySelector('#bruschetta-mic-permission-dialog'));

    toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#bruschetta-mic-permission-toggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);
    assertTrue(subpage.get(MIC_ALLOWED_PREF_PATH));
  });

  test('Navigate to shared USB devices', async () => {
    const link = subpage.shadowRoot!.querySelector<HTMLElement>(
        '#bruschettaSharedUsbDevicesRow');
    assertTrue(!!link);
    link.click();
    flush();

    assertEquals(
        routes.BRUSCHETTA_SHARED_USB_DEVICES,
        Router.getInstance().currentRoute);

    // Navigate back
    const popStateEventPromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await popStateEventPromise;
    await waitAfterNextRender(subpage);

    assertEquals(
        link, subpage.shadowRoot!.activeElement, `${link} should be focused.`);
  });

  test('Navigate to shared paths', async () => {
    const link = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#bruschettaSharedPathsRow');
    assertTrue(!!link);
    link.click();
    flush();

    assertEquals(
        routes.BRUSCHETTA_SHARED_PATHS, Router.getInstance().currentRoute);

    // Navigate back
    const popStateEventPromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await popStateEventPromise;
    await waitAfterNextRender(subpage);

    assertEquals(
        link, subpage.shadowRoot!.activeElement, `${link} should be focused.`);
  });

  test('Removing bruschetta navigates to the previous route', async () => {
    const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#remove > cr-button');
    assertTrue(!!button);
    assertFalse(button.disabled);
    button.click();
    flush();

    assertEquals(
        1,
        crostiniBrowserProxy.getCallCount('requestBruschettaUninstallerView'));
    setBruschettaPrefs(false, {bruschettaInstalled: false});
    await eventToPromise('popstate', window);
  });
});
