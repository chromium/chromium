// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('DisplayAndMagnificationPageTests', function() {
  let page = null;

  function initPage() {
    page = document.createElement('settings-display-and-magnification-page');
    document.body.appendChild(page);
  }

  setup(function() {
    PolymerTest.clearBody();
    loadTimeData.overrideValues(
        {isAccessibilityOSSettingsVisibilityEnabled: true});
    Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  [{selector: '#displaySubpageButton', route: routes.DISPLAY},
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
          assertEquals(route, router.currentRoute);
          assertNotEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should not be focused`);

          const popStateEventPromise = eventToPromise('popstate', window);
          router.navigateToPreviousRoute();
          await popStateEventPromise;
          await waitBeforeNextRender(page);

          assertEquals(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, router.currentRoute);
          assertEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should be focused`);
        });
  });

  test('no subpages are available in kiosk mode', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    initPage();
    flush();

    const subpageLinks = page.root.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });
});
