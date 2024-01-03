// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronCollapseElement, SettingsGetMostChromePageElement} from 'chrome://settings/lazy_load.js';
import {buildRouter, HatsBrowserProxyImpl, loadTimeData, Router, SettingsRoutes, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';


/** @fileoverview Suite of tests for get_most_chrome_page. */
suite('GetMostChromePage', function() {
  let routes: SettingsRoutes;
  let hatsBrowserProxy: TestHatsBrowserProxy;
  let testElement: SettingsGetMostChromePageElement;

  setup(function() {
    hatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(hatsBrowserProxy);

    loadTimeData.overrideValues({showGetTheMostOutOfChromeSection: true});
    Router.resetInstanceForTesting(buildRouter());
    routes = Router.getInstance().getRoutes();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-get-most-chrome-page');
    Router.getInstance().navigateTo(routes.GET_MOST_CHROME);
    document.body.appendChild(testElement);
    flush();
  });

  test('Basic', function() {
    const rows = testElement.shadowRoot!.querySelectorAll('cr-expand-button');
    assertTrue(rows.length > 0);
    rows.forEach((row) => {
      const ironCollapse = row.nextElementSibling as IronCollapseElement;
      assertTrue(!!ironCollapse);

      assertFalse(ironCollapse.opened);
      row.click();
      assertTrue(ironCollapse.opened);
    });
  });

  test('HatsSurveyRequested', async function() {
    const result =
        await hatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.OPENED_GET_MOST_CHROME, result);
  });
});
