// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://default-browser-modal/app.js';

import type {DefaultBrowserModalAppElement} from 'chrome://default-browser-modal/app.js';
import {BrowserProxy} from 'chrome://default-browser-modal/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestDefaultBrowserBrowserProxy} from './test_browser_proxy.js';

suite('DefaultBrowserModalAppWithIllustrationTest', function() {
  let app: DefaultBrowserModalAppElement;
  let browserProxy: TestDefaultBrowserBrowserProxy;

  setup(function() {
    browserProxy = new TestDefaultBrowserBrowserProxy();
    BrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      useSettingsIllustration: true,
      confirmButton: 'Open settings',
      tryAgainButton: 'Try again',
      cancelButton: 'Set later',
      done: 'Done',
      isStickyModal: false,
    });
    app = document.createElement('default-browser-modal-app');
    document.body.appendChild(app);
  });

  test('CheckLayout', async function() {
    assertTrue(app.useSettingsIllustration);

    const icon = app.shadowRoot.querySelector('#icon');
    assertTrue(isVisible(icon));
    assertEquals('chrome_logo.svg', icon!.getAttribute('src'));

    const headerBackground = app.shadowRoot.querySelector('#header-background');
    assertFalse(isVisible(headerBackground));

    const illustration =
        app.shadowRoot.querySelector<HTMLElement>('#illustration');
    assertTrue(!!illustration);

    const illustrationImg = illustration.querySelector('img');
    assertTrue(!!illustrationImg);

    const illustrationImgLoadPromise = eventToPromise('load', illustrationImg);
    await illustrationImgLoadPromise;

    assertTrue(isVisible(illustration));
    assertTrue(isVisible(illustrationImg));
    assertEquals(
        'settings_illustration.svg', illustrationImg.getAttribute('src'));
  });

  test('ClickSetDefault', function() {
    const confirmButton =
        app.shadowRoot.querySelector<HTMLElement>('#confirm-button');
    assertTrue(isVisible(confirmButton));
    assertTrue(!!confirmButton);
    confirmButton.click();
    assertEquals(1, browserProxy.handler.getCallCount('confirm'));
  });

  test('ClickCancel', function() {
    const cancelButton =
        app.shadowRoot.querySelector<HTMLElement>('#cancel-button');
    assertTrue(isVisible(cancelButton));
    assertTrue(!!cancelButton);
    cancelButton.click();
    assertEquals(1, browserProxy.handler.getCallCount('cancel'));
  });

  test('StickyModalTryAgainFlow', async function() {
    loadTimeData.overrideValues({isStickyModal: true});

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('default-browser-modal-app');
    document.body.appendChild(app);
    await app.updateComplete;

    const confirmButton =
        app.shadowRoot.querySelector<HTMLElement>('#confirm-button');
    const cancelButton =
        app.shadowRoot.querySelector<HTMLElement>('#cancel-button');
    assertTrue(isVisible(confirmButton));
    assertTrue(isVisible(cancelButton));
    assertEquals(
        loadTimeData.getString('confirmButton'),
        confirmButton!.textContent.trim());
    assertEquals(
        loadTimeData.getString('cancelButton'),
        cancelButton!.textContent.trim());

    confirmButton!.click();
    assertEquals(1, browserProxy.handler.getCallCount('confirm'));
    assertEquals(0, browserProxy.handler.getCallCount('tryAgain'));

    await app.updateComplete;
    assertTrue(isVisible(
        app.shadowRoot.querySelector<HTMLElement>('#confirm-button')));

    browserProxy.callbackRouterRemote.onHasAcceptedChanged(true);
    await browserProxy.callbackRouterRemote.$.flushForTesting();
    await app.updateComplete;

    const tryAgainButton =
        app.shadowRoot.querySelector<HTMLElement>('#try-again-button');
    const closeButton =
        app.shadowRoot.querySelector<HTMLElement>('#close-button');
    assertTrue(isVisible(tryAgainButton));
    assertTrue(isVisible(closeButton));
    assertEquals(
        loadTimeData.getString('tryAgainButton'),
        tryAgainButton!.textContent.trim());
    assertEquals(
        loadTimeData.getString('close'), closeButton!.textContent.trim());

    window.dispatchEvent(new Event('focus'));
    await browserProxy.handler.whenCalled('checkDefaultStatusAndMaybeClose');

    tryAgainButton!.click();
    assertEquals(1, browserProxy.handler.getCallCount('tryAgain'));

    closeButton!.click();
    assertEquals(1, browserProxy.handler.getCallCount('cancel'));
  });
});
