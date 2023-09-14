// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PrintingSettingsCardElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<printing-settings-card>', () => {
  let printingSettingsCard: PrintingSettingsCardElement;

  setup(async () => {
    printingSettingsCard = document.createElement('printing-settings-card');
    assert(printingSettingsCard);
    document.body.appendChild(printingSettingsCard);
    await flushTasks();
  });

  teardown(() => {
    printingSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to print jobs', async () => {
    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kPrintJobs.toString());
    Router.getInstance().navigateTo(routes.OS_PRINTING, params);

    flush();

    const deepLinkElement =
        printingSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            '#printManagement');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, printingSettingsCard.shadowRoot!.activeElement,
        'Print jobs button should be focused for settingId=1402.');
  });

  test('Deep link to scanning app', async () => {
    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kScanningApp.toString());
    Router.getInstance().navigateTo(routes.OS_PRINTING, params);

    flush();

    const deepLinkElement =
        printingSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            '#scanningApp');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, printingSettingsCard.shadowRoot!.activeElement,
        'Scanning app button should be focused for settingId=1403.');
  });

  test('Printers row is focused after returning from subpage', async () => {
    Router.getInstance().navigateTo(routes.OS_PRINTING);

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
});
