// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsPrintingPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('<os-settings-printing-page>', function() {
  let printingPage: OsSettingsPrintingPageElement;

  setup(async function() {
    printingPage = document.createElement('os-settings-printing-page');
    assert(printingPage);
    document.body.appendChild(printingPage);
    await flushTasks();
  });

  teardown(function() {
    printingPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to print jobs', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1402');
    Router.getInstance().navigateTo(routes.OS_PRINTING, params);

    flush();

    const deepLinkElement =
        printingPage.shadowRoot!.querySelector('#printManagement')!.shadowRoot!
            .querySelector('cr-icon-button');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Print jobs button should be focused for settingId=1402.');
  });

  test('Deep link to scanning app', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1403');
    Router.getInstance().navigateTo(routes.OS_PRINTING, params);

    flush();

    const deepLinkElement =
        printingPage.shadowRoot!.querySelector('#scanningApp')!.shadowRoot!
            .querySelector('cr-icon-button');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Scanning app button should be focused for settingId=1403.');
  });
});
