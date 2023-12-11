// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {BruschettaSubpageElement, CrostiniBrowserProxyImpl, CrostiniPortSetting, SettingsCrostiniPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

let crostiniPage: SettingsCrostiniPageElement;
let subpage: BruschettaSubpageElement;
let crostiniBrowserProxy: TestCrostiniBrowserProxy;

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

function setCrostiniPrefs(enabled: boolean, {
  sharedPaths = {},
  forwardedPorts = [],
  micAllowed = false,
  arcEnabled = false,
  bruschettaInstalled = false,
}: PrefParams = {}): void {
  crostiniPage.prefs = {
    arc: {
      enabled: {value: arcEnabled},
    },
    bruschetta: {
      installed: {
        value: bruschettaInstalled,
      },
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

suite('<settings-bruschetta-subpage>', () => {
  setup(async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);

    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    flush();

    disableAnimationsAndTransitions();

    Router.getInstance().navigateTo(routes.CROSTINI);
    setCrostiniPrefs(false, {bruschettaInstalled: true});
    flush();

    const crostiniSettingsCard =
        crostiniPage.shadowRoot!.querySelector('crostini-settings-card');
    assertTrue(!!crostiniSettingsCard);
    crostiniSettingsCard.set('showBruschetta_', true);
    await flushTasks();
    const button =
        crostiniSettingsCard.shadowRoot!.querySelector<HTMLButtonElement>(
            '#bruschetta');
    assertTrue(!!button);
    button.click();

    await flushTasks();
    const subpageElement =
        crostiniPage.shadowRoot!.querySelector('settings-bruschetta-subpage');
    assertTrue(!!subpageElement);
    subpage = subpageElement;
  });

  teardown(() => {
    crostiniPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Navigate to shared USB devices', async () => {
    const link = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#bruschettaSharedUsbDevicesRow');
    assertTrue(!!link);
    link.click();
    flush();

    assertEquals(
        routes.BRUSCHETTA_SHARED_USB_DEVICES,
        Router.getInstance().currentRoute);

    assertTrue(!!crostiniPage.shadowRoot!.querySelector(
        'settings-guest-os-shared-usb-devices[guest-os-type="bruschetta"]'));
    // Functionality is tested in guest_os_shared_usb_devices_test.js

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

    assertTrue(!!crostiniPage.shadowRoot!.querySelector(
        'settings-guest-os-shared-paths[guest-os-type="bruschetta"]'));
    // Functionality is tested in guest_os_shared_paths_test.js

    // Navigate back
    const popStateEventPromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await popStateEventPromise;
    await waitAfterNextRender(subpage);

    assertEquals(
        link, subpage.shadowRoot!.activeElement, `${link} should be focused.`);
  });

  test('Remove bruschetta', async () => {
    const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#remove cr-button');
    assertTrue(!!button);
    assertFalse(button.disabled);
    button.click();
    flush();

    assertEquals(
        1,
        crostiniBrowserProxy.getCallCount('requestBruschettaUninstallerView'));
    setCrostiniPrefs(false, {bruschettaInstalled: false});

    await eventToPromise('popstate', window);

    assertEquals(routes.CROSTINI, Router.getInstance().currentRoute);

    const crostiniSettingsCard =
        crostiniPage.shadowRoot!.querySelector('crostini-settings-card');
    assertTrue(!!crostiniSettingsCard);
    assertTrue(!!crostiniSettingsCard.shadowRoot!.querySelector(
        '#enableBruschettaButton'));
  });
});
