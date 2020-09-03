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
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('PrintingPageTests', function() {
  /** @type {SettingsPrintingPageElement} */
  let printingPage = null;

  setup(function() {
    PolymerTest.clearBody();
    printingPage = document.createElement('os-settings-printing-page');
    document.body.appendChild(printingPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    printingPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to print jobs', async () => {
    loadTimeData.overrideValues({
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
});