// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('PrintingPageTests', function() {
  /** @type {SettingsPrintingPageElement} */
  let printingPage = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    printingPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  /**
   * Set up printing page with loadTimeData overrides
   * @param {Object} overrides Dictionary of objects to override.
   * @return {!Promise}
   */
  function initializePrintingPage(overrides) {
    loadTimeData.overrideValues(overrides);
    printingPage = /** @type {!SettingsPrintingPageElement} */ (
        document.createElement('os-settings-printing-page'));
    assertTrue(!!printingPage);
    document.body.appendChild(printingPage);
    return test_util.flushTasks();
  }

  test('Deep link to print jobs', async () => {
    await initializePrintingPage({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '1402');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_PRINTING, params);

    Polymer.dom.flush();

    const deepLinkElement =
        printingPage.$$('#printManagement').$$('cr-icon-button');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Print jobs button should be focused for settingId=1402.');
  });

  test('Deep link to scanning app', async () => {
    await initializePrintingPage({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '1403');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_PRINTING, params);

    Polymer.dom.flush();

    const deepLinkElement =
        printingPage.$$('#scanningApp').$$('cr-icon-button');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Scanning app button should be focused for settingId=1403.');
  });
});