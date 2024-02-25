// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrostiniBrowserProxyImpl, CrostiniPortSetting, CrostiniSettingsCardElement, GuestOsBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';
import {clearBody} from '../utils.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

suite('<crostini-settings-card>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  const hostRoute = isRevampWayfindingEnabled ? routes.ABOUT : routes.CROSTINI;

  let crostiniSettingsCard: CrostiniSettingsCardElement;
  let guestOsBrowserProxy: TestGuestOsBrowserProxy;
  let crostiniBrowserProxy: TestCrostiniBrowserProxy;

  interface PrefParams {
    sharedPaths?: {[key: string]: string[]};
    forwardedPorts?: CrostiniPortSetting[];
    micAllowed?: boolean;
    arcEnabled?: boolean;
    bruschettaInstalled?: boolean;
  }

  function setCrostiniPrefs(enabled: boolean,
    {
      sharedPaths = {},
      forwardedPorts = [],
      micAllowed = false,
      arcEnabled = false,
      bruschettaInstalled = false,
    }: PrefParams = {}): void {
    crostiniSettingsCard.prefs = {
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

  async function createCrostiniSettingsCard(): Promise<void> {
    clearBody();
    crostiniSettingsCard = document.createElement('crostini-settings-card');
    document.body.appendChild(crostiniSettingsCard);
    setCrostiniPrefs(false);
    await flushTasks();
  }

  setup(() => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
      showBruschetta: false,
    });

    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);

    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);

    Router.getInstance().navigateTo(hostRoute);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  test('NotSupported', async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: false,
      isCrostiniSupported: false,
    });
    await createCrostiniSettingsCard();

    assertTrue(!!crostiniSettingsCard.shadowRoot!.querySelector(
        '#enableCrostiniButton'));
    assertNull(
        crostiniSettingsCard.shadowRoot!.querySelector('cr-policy-indicator'));
  });

  test('NotAllowed', async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: false,
      isCrostiniSupported: true,
    });
    await createCrostiniSettingsCard();

    assertTrue(!!crostiniSettingsCard.shadowRoot!.querySelector(
        '#enableCrostiniButton'));
    assertTrue(
      !!crostiniSettingsCard.shadowRoot!.
        querySelector('cr-policy-indicator'));
  });

  test('Enable', async () => {
    await createCrostiniSettingsCard();

    const button =
      crostiniSettingsCard.shadowRoot!.
        querySelector<HTMLButtonElement>('#enableCrostiniButton');
    assertTrue(!!button);
    assertNull(
      crostiniSettingsCard.shadowRoot!.querySelector('.subpage-arrow'));
    assertFalse(button.disabled);

    button.click();
    flush();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('requestCrostiniInstallerView'));
    setCrostiniPrefs(true);

    assertTrue(
        !!crostiniSettingsCard.shadowRoot!.querySelector('.subpage-arrow'));
  });

  test('ButtonDisabledDuringInstall', async () => {
    await createCrostiniSettingsCard();

    const button =
      crostiniSettingsCard.shadowRoot!.
        querySelector<HTMLButtonElement>('#enableCrostiniButton');
    assertTrue(!!button);

    await flushTasks();
    assertFalse(button.disabled);
    webUIListenerCallback('crostini-installer-status-changed', true);

    await flushTasks();
    assertTrue(button.disabled);
    webUIListenerCallback('crostini-installer-status-changed', false);

    await flushTasks();
    assertFalse(button.disabled);
  });

  test('Deep link to setup Crostini', async () => {
    await createCrostiniSettingsCard();

    const params = new URLSearchParams();
    const setUpCrostiniSettingId =
        settingMojom.Setting.kSetUpCrostini.toString();
    params.append('settingId', setUpCrostiniSettingId);
    Router.getInstance().navigateTo(hostRoute, params);

    const deepLinkElement =
      crostiniSettingsCard.shadowRoot!.
        querySelector<HTMLButtonElement>('#enableCrostiniButton');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Enable Crostini button should be focused for settingId=${
            setUpCrostiniSettingId}.`);
  });

  test(
      'Crostini details row is focused when returning from subpage',
      async () => {
        await createCrostiniSettingsCard();
        setCrostiniPrefs(true);
        flush();

        const triggerSelector = '#crostini .subpage-arrow';
        const subpageTrigger =
            crostiniSettingsCard.shadowRoot!.querySelector<HTMLButtonElement>(
                triggerSelector);
        assertTrue(!!subpageTrigger);

        // Sub-page trigger navigates to subpage for route
        subpageTrigger.click();
        assertEquals(
            routes.CROSTINI_DETAILS, Router.getInstance().currentRoute);

        // Navigate back
        const popStateEventPromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await popStateEventPromise;
        await waitAfterNextRender(crostiniSettingsCard);

        assertEquals(
            subpageTrigger, crostiniSettingsCard.shadowRoot!.activeElement,
            `${triggerSelector} should be focused.`);
      });

  suite('when Bruschetta is available', () => {
    setup(() => {
      loadTimeData.overrideValues({showBruschetta: true});
    });

    test('Install Bruschetta', async () => {
      await createCrostiniSettingsCard();
      setCrostiniPrefs(false, {bruschettaInstalled: false});
      flush();

      const installSelector = '#enableBruschettaButton';
      const subpageSelector = '#bruschetta .subpage-arrow';
      const installButton =
          crostiniSettingsCard.shadowRoot!.querySelector<HTMLButtonElement>(
              installSelector);
      assertTrue(!!installButton);
      assertFalse(installButton.disabled);
      assertNull(
          crostiniSettingsCard.shadowRoot!.querySelector(subpageSelector));

      installButton.click();
      flush();

      assertEquals(
          1,
          crostiniBrowserProxy.getCallCount('requestBruschettaInstallerView'));
      setCrostiniPrefs(false, {bruschettaInstalled: true});

      assertTrue(
          !!crostiniSettingsCard.shadowRoot!.querySelector(subpageSelector));
    });

    test('Navigate to bruschetta subpage', async () => {
      await createCrostiniSettingsCard();
      setCrostiniPrefs(false, {bruschettaInstalled: true});
      flush();

      const subpageSelector = '#bruschetta .subpage-arrow';
      const subpageButton =
          crostiniSettingsCard.shadowRoot!.querySelector<HTMLButtonElement>(
              subpageSelector);
      assertTrue(!!subpageButton);

      subpageButton.click();
      assertEquals(
          routes.BRUSCHETTA_DETAILS, Router.getInstance().currentRoute);
    });

    test(
        'Bruschetta details row is focused when returning from subpage',
        async () => {
          await createCrostiniSettingsCard();
          setCrostiniPrefs(true, {bruschettaInstalled: true});
          flush();

          const triggerSelector = '#bruschetta .subpage-arrow';
          const subpageTrigger =
              crostiniSettingsCard.shadowRoot!.querySelector<HTMLButtonElement>(
                  triggerSelector);
          assertTrue(!!subpageTrigger);

          // Sub-page trigger navigates to subpage for route
          subpageTrigger.click();
          assertEquals(
              routes.BRUSCHETTA_DETAILS, Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(crostiniSettingsCard);

          assertEquals(
              subpageTrigger, crostiniSettingsCard.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });

  suite('when Bruschetta is not available', () => {
    setup(() => {
      loadTimeData.overrideValues({showBruschetta: false});
    });

    test('Bruschetta row is not stamped', async () => {
      await createCrostiniSettingsCard();

      const bruschettaRow =
          crostiniSettingsCard.shadowRoot!.querySelector<HTMLElement>(
              '#bruschetta');
      assertNull(bruschettaRow);
    });
  });
});
