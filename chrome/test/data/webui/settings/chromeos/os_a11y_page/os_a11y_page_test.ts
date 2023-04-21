// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';
import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, OsA11yPageBrowserProxyImpl, OsSettingsA11yPageElement, Router, routes, SettingsPrefsElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestOsA11yPageBrowserProxy} from './test_os_a11y_page_browser_proxy.js';

suite('<os-settings-a11y-page>', () => {
  let page: OsSettingsA11yPageElement;
  let prefElement: SettingsPrefsElement;
  let browserProxy: TestOsA11yPageBrowserProxy;

  setup(async () => {
    browserProxy = new TestOsA11yPageBrowserProxy();
    OsA11yPageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('os-settings-a11y-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to always show a11y settings', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1500');
    Router.getInstance().navigateTo(routes.OS_ACCESSIBILITY, params);

    flush();

    const deepLinkElement =
        page.shadowRoot!.querySelector('#optionsInMenuToggle')!.shadowRoot!
            .querySelector('cr-toggle');
    assert(deepLinkElement);
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
        page.shadowRoot!.querySelector<HTMLElement>('#a11yImageLabels');
    a11yImageLabelsToggle!.click();
    flush();

    // Make sure confirmA11yImageLabels is called.
    assertEquals(1, browserProxy.getCallCount('confirmA11yImageLabels'));
  });

  test('Checking pdf ocr toggle visibility in the TTS page', async () => {
    // Need to have this test here as the screen reader state is passed from
    // the os-settings-a11y-page to the settings-text-to-speech-page.
    // `features::kPdfOcr` is enabled in os_settings_v3_browsertest.js
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));

    Router.getInstance().navigateTo(routes.A11Y_TEXT_TO_SPEECH);
    flush();
    const ttsPage =
        page.shadowRoot!.querySelector('settings-text-to-speech-page');

    // Disable ChromeVox to hide the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', false);

    const pdfOcrToggle =
        ttsPage!.shadowRoot!.querySelector<HTMLElement>('#crosPdfOcrToggle');
    assert(pdfOcrToggle);
    await waitAfterNextRender(pdfOcrToggle);
    assertTrue(pdfOcrToggle.hidden);

    // Enable ChromeVox to show the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', true);

    await waitAfterNextRender(pdfOcrToggle);
    assertFalse(pdfOcrToggle.hidden);
  });
});
