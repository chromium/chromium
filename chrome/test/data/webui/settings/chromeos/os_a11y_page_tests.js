// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, OsA11yPageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestOsA11yPageBrowserProxy} from './test_os_a11y_page_browser_proxy.js';

suite('A11yPageTests', function() {
  /** @type {SettingsA11yPageElement} */
  let page = null;
  let browserProxy = null;

  setup(async function() {
    browserProxy = new TestOsA11yPageBrowserProxy();
    OsA11yPageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('os-settings-a11y-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to always show a11y settings', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1500');
    Router.getInstance().navigateTo(
        routes.OS_ACCESSIBILITY, params);

    flush();

    const deepLinkElement =
        page.shadowRoot.querySelector('#optionsInMenuToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Always show a11y toggle should be focused for settingId=1500.');
  });

  test('Turning on get image descriptions from Google launches dialog', () => {
    // Enable ChromeVox to show toggle.
    page.setPrefValue('settings.accessibility', true);

    // Turn on 'Get image descriptions from Google'.
    const a11yImageLabelsToggle =
        page.shadowRoot.querySelector('#a11yImageLabels');
    a11yImageLabelsToggle.click();
    flush();

    // Make sure confirmA11yImageLabels is called.
    assertEquals(browserProxy.getCallCount('confirmA11yImageLabels'), 1);
  });
});
