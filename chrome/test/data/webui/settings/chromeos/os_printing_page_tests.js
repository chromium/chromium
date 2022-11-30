// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('PrintingPageTests', function() {
  /** @type {SettingsPrintingPageElement} */
  let printingPage = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    printingPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Set up printing page with loadTimeData overrides
   * @return {!Promise}
   */
  function initializePrintingPage() {
    printingPage = /** @type {!SettingsPrintingPageElement} */ (
        document.createElement('os-settings-printing-page'));
    assertTrue(!!printingPage);
    document.body.appendChild(printingPage);
    return flushTasks();
  }

  test('Deep link to print jobs', async () => {
    await initializePrintingPage();
    const params = new URLSearchParams();
    params.append('settingId', '1402');
    Router.getInstance().navigateTo(routes.OS_PRINTING, params);

    flush();

    const deepLinkElement =
        printingPage.shadowRoot.querySelector('#printManagement')
            .shadowRoot.querySelector('cr-icon-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Print jobs button should be focused for settingId=1402.');
  });

  test('Deep link to scanning app', async () => {
    await initializePrintingPage();

    const params = new URLSearchParams();
    params.append('settingId', '1403');
    Router.getInstance().navigateTo(routes.OS_PRINTING, params);

    flush();

    const deepLinkElement =
        printingPage.shadowRoot.querySelector('#scanningApp')
            .shadowRoot.querySelector('cr-icon-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Scanning app button should be focused for settingId=1403.');
  });
});
