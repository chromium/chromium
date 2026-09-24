// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/intro_app.js';

import {IntroBrowserProxyImpl} from 'chrome://intro/browser_proxy.js';
import type {IntroAppElement} from 'chrome://intro/intro_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestIntroBrowserProxy} from './test_intro_browser_proxy.js';

suite('DiceAppTest', function() {
  let testElement: IntroAppElement;
  let testBrowserProxy: TestIntroBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestIntroBrowserProxy();
    IntroBrowserProxyImpl.setInstance(testBrowserProxy);
    loadTimeData.overrideValues({disableAnimations: false});
  });

  async function createIntroAppElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('intro-app');
    document.body.appendChild(testElement);
    await waitBeforeNextRender(testElement);
  }

  function isSelectorActive(selector: string) {
    return testElement.shadowRoot.querySelector(selector)!.classList.contains(
        'active');
  }

  test('"splash" is the active view', async function() {
    await createIntroAppElement();
    assertTrue(isSelectorActive('#splash'));
    assertFalse(isSelectorActive('sign-in-promo'));
  });

  test(
      '"signInPromo" is the active view when animations disabled',
      async function() {
        loadTimeData.overrideValues({disableAnimations: true});
        await createIntroAppElement();

        assertFalse(isSelectorActive('#splash'));
        assertTrue(isSelectorActive('sign-in-promo'));
      });
});
