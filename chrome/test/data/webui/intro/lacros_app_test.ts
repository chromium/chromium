// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/lacros_app.js';

import {IntroBrowserProxyImpl} from 'chrome://intro/browser_proxy.js';
import type {LacrosIntroAppElement} from 'chrome://intro/lacros_app.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestIntroBrowserProxy} from './test_intro_browser_proxy.js';

suite('LacrosAppTest', function() {
  let app: LacrosIntroAppElement;
  let browserProxy: TestIntroBrowserProxy;

  const AVATAR_URL: string = 'chrome://theme/IDR_PROFILE_AVATAR_1';

  setup(async function() {
    browserProxy = new TestIntroBrowserProxy();
    IntroBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('intro-app');
    document.body.appendChild(app);
    await waitBeforeNextRender(app);
    return browserProxy.whenCalled('initializeMainView');
  });

  teardown(function() {
    app.remove();
  });

  /**
   * Checks that the expected image url is displayed.
   */
  function checkImageUrl(expectedUrl: string) {
    const img = app.shadowRoot!.querySelector<HTMLImageElement>('#avatar')!;
    assertEquals(expectedUrl, img.src);
  }

  test('proceed button clicked', function() {
    assertFalse(app.$.proceedButton.disabled);
    app.$.proceedButton.click();
    assertTrue(app.$.proceedButton.disabled);
    return browserProxy.whenCalled('continueWithAccount');
  });

  test('on-profile-info-changed event', function() {
    // Helper to test all the text values in the UI.
    function checkTextValues(expectedTitle: string, expectedSubtitle: string) {
      const titleElement = app.shadowRoot!.querySelector('.title')!;
      assertEquals(expectedTitle, titleElement.textContent!.trim());

      const subtitleElement = app.shadowRoot!.querySelector('.subtitle')!;
      assertEquals(expectedSubtitle, subtitleElement.textContent!.trim());
    }

    // Initial values.
    checkTextValues('', '');
    checkImageUrl('');

    // Update the values.
    webUIListenerCallback('on-profile-info-changed', {
      pictureUrl: AVATAR_URL,
      title: 'new_title',
      subtitle: 'new_subtitle',
      managementDisclaimer: 'new_disclaimer',
    });

    checkTextValues('new_title', 'new_subtitle');
    checkImageUrl(AVATAR_URL);
  });

  test('management badge and notice displayed for managed devices', function() {
    webUIListenerCallback('on-profile-info-changed', {
      managementDisclaimer: '',
    });

    assertFalse(isChildVisible(app, '#info-box'));
    assertFalse(isChildVisible(app, '.work-badge'));

    webUIListenerCallback('on-profile-info-changed', {
      managementDisclaimer: 'management-disclaimer',
    });

    assertTrue(isChildVisible(app, '#info-box'));
    assertTrue(isChildVisible(app, '.work-badge'));
  });
});
