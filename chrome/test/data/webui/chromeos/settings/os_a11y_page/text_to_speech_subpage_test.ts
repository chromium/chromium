// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsTextToSpeechSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-text-to-speech-subpage>', function() {
  let page: SettingsTextToSpeechSubpageElement;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-text-to-speech-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(function() {
    Router.getInstance().navigateTo(routes.A11Y_TEXT_TO_SPEECH);
  });

  teardown(function() {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  [{selector: '#ttsSubpageButton', route: routes.MANAGE_TTS_SETTINGS},
  ].forEach(({selector, route}) => {
    test(
        `should focus ${selector} button when returning from ${
            route.path} subpage`,
        async () => {
          await initPage();
          const router = Router.getInstance();

          const subpageButton =
              page.shadowRoot!.querySelector<HTMLElement>(selector);
          assert(subpageButton);

          subpageButton.click();
          assertEquals(route, router.currentRoute);
          assertNotEquals(
              subpageButton, page.shadowRoot!.activeElement,
              `${selector} should not be focused`);

          const popStateEventPromise = eventToPromise('popstate', window);
          router.navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(page);

          assertEquals(routes.A11Y_TEXT_TO_SPEECH, router.currentRoute);
          assertEquals(
              subpageButton, page.shadowRoot!.activeElement,
              `${selector} should be focused`);
        });
  });

  test('only allowed subpages are available in kiosk mode', async function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();

    const allowed_subpages = [
      'chromeVoxSubpageButton',
      'selectToSpeakSubpageButton',
      'ttsSubpageButton',
    ];

    const subpages = page.root!.querySelectorAll('cr-link-row');
    subpages.forEach(function(subpage) {
      if (isVisible(subpage)) {
        assertTrue(allowed_subpages.includes(subpage.id));
      }
    });
  });
});
