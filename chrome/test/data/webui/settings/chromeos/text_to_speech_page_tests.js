// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

suite('TextToSpeechPageTests', function() {
  let page = null;

  function initPage(opt_prefs) {
    page = document.createElement('settings-text-to-speech-page');
    page.prefs = opt_prefs || getDefaultPrefs();
    document.body.appendChild(page);
  }

  function getDefaultPrefs() {
    return {
      'settings': {
        'accessibility': {
          key: 'settings.accessibility',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
    };
  }

  setup(function() {
    PolymerTest.clearBody();
    loadTimeData.overrideValues(
      {isAccessibilityOSSettingsVisibilityEnabled: true});
    Router.getInstance().navigateTo(routes.A11Y_TEXT_TO_SPEECH);
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  [{selector: '#ttsSubpageButton', route: routes.MANAGE_TTS_SETTINGS},
  ].forEach(({selector, route}) => {
    test(
        `should focus ${selector} button when returning from ${
            route.path} subpage`,
        async () => {
          initPage();
          flush();
          const router = Router.getInstance();

          const subpageButton = page.shadowRoot.querySelector(selector);
          assertTrue(!!subpageButton);

          subpageButton.click();
          assertEquals(route, router.getCurrentRoute());
          assertNotEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should not be focused`);

          const popStateEventPromise = eventToPromise('popstate', window);
          router.navigateToPreviousRoute();
          await popStateEventPromise;
          await waitBeforeNextRender(page);

          assertEquals(routes.A11Y_TEXT_TO_SPEECH, router.getCurrentRoute());
          assertEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should be focused`);
        });
  });
});
