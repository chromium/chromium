// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {BruschettaSubpageElement, CrostiniBrowserProxyImpl, CrostiniPortSetting} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

  function setCrostiniPrefs(enabled: boolean, {
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
    setCrostiniPrefs(false, {bruschettaInstalled: true});
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
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
    setCrostiniPrefs(false, {bruschettaInstalled: false});
    await eventToPromise('popstate', window);
  });
});
