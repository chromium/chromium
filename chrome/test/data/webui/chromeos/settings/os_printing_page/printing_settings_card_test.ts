// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PrintingSettingsCardElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<printing-settings-card>', () => {
  let printingSettingsCard: PrintingSettingsCardElement;
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  const defaultRoute =
      isRevampWayfindingEnabled ? routes.DEVICE : routes.OS_PRINTING;


  setup(async () => {
    printingSettingsCard = document.createElement('printing-settings-card');
    assert(printingSettingsCard);
    document.body.appendChild(printingSettingsCard);
    await flushTasks();

    Router.getInstance().navigateTo(defaultRoute);
  });

  teardown(() => {
    printingSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  // When the revamp wayfinding is enabled, the print jobs is in the cups
  // printer page.
  if (!isRevampWayfindingEnabled) {
    test('Deep link to print jobs', async () => {
      const params = new URLSearchParams();
      const printJobsSettingId = settingMojom.Setting.kPrintJobs.toString();
      params.append('settingId', printJobsSettingId);
      Router.getInstance().navigateTo(defaultRoute, params);

      flush();

      const deepLinkElement =
          printingSettingsCard.shadowRoot!.querySelector<HTMLElement>(
              '#printManagement');
      assert(deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, printingSettingsCard.shadowRoot!.activeElement,
          `Print jobs button should be focused for settingId=${
              printJobsSettingId}.`);
    });
  }

  test('Printers row is focused after returning from subpage', async () => {
    const triggerSelector = '#cupsPrintersRow';
    const subpageTrigger =
        printingSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            triggerSelector);
    assert(subpageTrigger);

    // Sub-page trigger navigates to Printers subpage
    subpageTrigger.click();
    assertEquals(routes.CUPS_PRINTERS, Router.getInstance().currentRoute);

    // Navigate back
    const popStateEventPromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await popStateEventPromise;
    await waitAfterNextRender(printingSettingsCard);

    assertEquals(
        subpageTrigger, printingSettingsCard.shadowRoot!.activeElement,
        `${triggerSelector} should be focused.`);
  });

  test('Deep link to scanning app', async () => {
    const params = new URLSearchParams();
    const scanningAppSettingId = settingMojom.Setting.kScanningApp.toString();
    params.append('settingId', scanningAppSettingId);
    Router.getInstance().navigateTo(defaultRoute, params);

    flush();

    const deepLinkElement =
        printingSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            '#scanningApp');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, printingSettingsCard.shadowRoot!.activeElement,
        `Scanning app button should be focused for settingId=${
            scanningAppSettingId}.`);
  });
});
