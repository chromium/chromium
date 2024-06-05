// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {CrSettingsPrefs, OsA11yPageBrowserProxyImpl, OsSettingsA11yPageElement, OsSettingsRoutes, Router, routes, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestOsA11yPageBrowserProxy} from './test_os_a11y_page_browser_proxy.js';

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

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
        page.shadowRoot!.querySelector<HTMLElement>('#a11yImageLabelsToggle');
    a11yImageLabelsToggle!.click();
    flush();

    // Make sure confirmA11yImageLabels is called.
    assertEquals(1, browserProxy.getCallCount('confirmA11yImageLabels'));
  });

  const subpageTriggerData: SubpageTriggerData[] = [
    {
      triggerSelector: '#textToSpeechSubpageTrigger',
      routeName: 'A11Y_TEXT_TO_SPEECH',
    },
    {
      triggerSelector: '#displayAndMagnificationPageTrigger',
      routeName: 'A11Y_DISPLAY_AND_MAGNIFICATION',
    },
    {
      triggerSelector: '#keyboardAndTextInputPageTrigger',
      routeName: 'A11Y_KEYBOARD_AND_TEXT_INPUT',
    },
    {
      triggerSelector: '#cursorAndTouchpadPageTrigger',
      routeName: 'A11Y_CURSOR_AND_TOUCHPAD',
    },
    {
      triggerSelector: '#audioAndCaptionsPageTrigger',
      routeName: 'A11Y_AUDIO_AND_CAPTIONS',
    },
  ];
  subpageTriggerData.forEach(({triggerSelector, routeName}) => {
    test(
        `Row for ${routeName} is focused when returning from subpage`,
        async () => {
          Router.getInstance().navigateTo(routes.OS_ACCESSIBILITY);

          const subpageTrigger =
              page.shadowRoot!.querySelector<HTMLElement>(triggerSelector);
          assertTrue(!!subpageTrigger);

          // Sub-page trigger navigates to subpage for route
          subpageTrigger.click();
          assertEquals(routes[routeName], Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(page);

          assertEquals(
              subpageTrigger, page.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });
});
