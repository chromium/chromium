// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {HistoryCrossDeviceSigninPromoBrowserProxy} from 'chrome://history/history.js';
import type {HistoryCrossDeviceSigninPromoElement} from 'chrome://history/history.js';
import type {HistoryCrossDeviceSigninPromoHandlerRemote} from 'chrome://resources/cr_components/history/history_cross_device_signin_promo.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestHistoryCrossDeviceSigninPromoBrowserProxy extends
    HistoryCrossDeviceSigninPromoBrowserProxy {
  constructor() {
    super(
        new TestHistoryCrossDeviceSigninPromoHandlerRemote() as unknown as
        HistoryCrossDeviceSigninPromoHandlerRemote);
  }
}

class TestHistoryCrossDeviceSigninPromoHandlerRemote extends TestBrowserProxy {
  constructor() {
    super([
      'shouldShowPromoCard',
      'onPromoCardShown',
      'onPromoCardDismissed',
      'onPromoCardActionClicked',
    ]);
  }

  shouldShowPromoCard() {
    this.methodCalled('shouldShowPromoCard');
    return Promise.resolve({shouldShow: true});
  }

  onPromoCardShown() {
    this.methodCalled('onPromoCardShown');
  }

  onPromoCardDismissed() {
    this.methodCalled('onPromoCardDismissed');
  }

  onPromoCardActionClicked() {
    this.methodCalled('onPromoCardActionClicked');
    return Promise.resolve();
  }
}

suite('HistoryCrossDeviceSigninPromoTest', function() {
  let element: HistoryCrossDeviceSigninPromoElement;
  let testBrowserProxy: TestHistoryCrossDeviceSigninPromoBrowserProxy;
  let handlerRemote: TestHistoryCrossDeviceSigninPromoHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestHistoryCrossDeviceSigninPromoBrowserProxy();
    handlerRemote = testBrowserProxy.handler as unknown as
        TestHistoryCrossDeviceSigninPromoHandlerRemote;
    HistoryCrossDeviceSigninPromoBrowserProxy.setInstance(testBrowserProxy);

    element = document.createElement('history-cross-device-signin-promo');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('shouldShowPromoCard is called on connection', async () => {
    await handlerRemote.whenCalled('shouldShowPromoCard');
    assertEquals(1, handlerRemote.getCallCount('shouldShowPromoCard'));
    await handlerRemote.whenCalled('onPromoCardShown');
    assertEquals(1, handlerRemote.getCallCount('onPromoCardShown'));
  });

  test(
      'Clicking close button calls onPromoCardDismissed and fires event',
      async () => {
        let eventFired = false;
        let shouldShowVal = true;
        element.addEventListener(
            'should-show-history-cross-device-signin-promo', (e: Event) => {
              eventFired = true;
              shouldShowVal =
                  (e as CustomEvent<{shouldShow: boolean}>).detail.shouldShow;
            });

        element.$.close.click();
        await handlerRemote.whenCalled('onPromoCardDismissed');
        await microtasksFinished();

        assertEquals(1, handlerRemote.getCallCount('onPromoCardDismissed'));
        assertEquals(0, handlerRemote.getCallCount('onPromoCardActionClicked'));
        assertTrue(eventFired);
        assertFalse(shouldShowVal);
      });

  test(
      'Clicking action button disables button, calls onPromoCardActionClicked, and waits for action completion',
      async () => {
        let resolveActionPromise: () => void = () => {};
        handlerRemote.onPromoCardActionClicked = () => {
          handlerRemote.methodCalled('onPromoCardActionClicked');
          return new Promise<void>(resolve => {
            resolveActionPromise = resolve;
          });
        };

        let eventFired = false;
        let shouldShowVal = true;
        element.addEventListener(
            'should-show-history-cross-device-signin-promo', (e: Event) => {
              eventFired = true;
              shouldShowVal =
                  (e as CustomEvent<{shouldShow: boolean}>).detail.shouldShow;
            });

        assertFalse(element.$.actionButton.disabled);
        element.$.actionButton.click();
        await handlerRemote.whenCalled('onPromoCardActionClicked');
        await microtasksFinished();

        assertEquals(1, handlerRemote.getCallCount('onPromoCardActionClicked'));
        assertEquals(0, handlerRemote.getCallCount('onPromoCardDismissed'));
        assertTrue(element.$.actionButton.disabled);
        assertFalse(eventFired);

        resolveActionPromise();
        await microtasksFinished();

        assertTrue(eventFired);
        assertFalse(shouldShowVal);
      });
});
