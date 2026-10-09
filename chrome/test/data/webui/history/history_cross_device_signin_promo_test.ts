// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {HistoryCrossDeviceSigninPromoBrowserProxy} from 'chrome://history/history.js';
import type {HistoryCrossDeviceSigninPromoElement} from 'chrome://history/history.js';
import type {HistoryCrossDeviceSigninPromoHandlerRemote, HistoryCrossDeviceSigninPromoPageRemote} from 'chrome://resources/cr_components/history/history_cross_device_signin_promo.mojom-webui.js';
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
  page: HistoryCrossDeviceSigninPromoPageRemote|null = null;

  constructor() {
    super([
      'shouldShowPromoCard',
      'onPromoCardShown',
      'onPromoCardDismissed',
      'onPromoCardActionClicked',
      'setPage',
    ]);
  }

  setPage(page: HistoryCrossDeviceSigninPromoPageRemote) {
    this.page = page;
    this.methodCalled('setPage');
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

  function deferActionReply(): () => void {
    let resolveReply: () => void = () => {};
    handlerRemote.onPromoCardActionClicked = () => {
      handlerRemote.methodCalled('onPromoCardActionClicked');
      return new Promise<void>(resolve => {
        resolveReply = resolve;
      });
    };
    return () => resolveReply();
  }

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

  test('Action button click hides promo once action completes', async () => {
    const resolveAction = deferActionReply();

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
    // The button is only disabled by the browser's bubble state updates.
    assertFalse(element.$.actionButton.disabled);
    assertFalse(eventFired);

    resolveAction();
    await microtasksFinished();

    assertTrue(eventFired);
    assertFalse(shouldShowVal);
  });

  test('Bubble state changes toggle the action button', async () => {
    await handlerRemote.whenCalled('setPage');
    const page = handlerRemote.page!;
    assertFalse(element.$.actionButton.disabled);

    page.onPromoBubbleStateChanged(true);
    await page.$.flushForTesting();
    await microtasksFinished();
    assertTrue(element.$.actionButton.disabled);

    page.onPromoBubbleStateChanged(false);
    await page.$.flushForTesting();
    await microtasksFinished();
    assertFalse(element.$.actionButton.disabled);
  });

  test('Reconnecting rebinds the page', async () => {
    await handlerRemote.whenCalled('setPage');
    assertEquals(1, handlerRemote.getCallCount('setPage'));

    handlerRemote.page!.onPromoBubbleStateChanged(true);
    await handlerRemote.page!.$.flushForTesting();
    await microtasksFinished();
    assertTrue(element.$.actionButton.disabled);

    element.remove();
    await microtasksFinished();

    document.body.appendChild(element);
    await microtasksFinished();
    assertEquals(2, handlerRemote.getCallCount('setPage'));
    assertFalse(element.$.actionButton.disabled);

    handlerRemote.page!.onPromoBubbleStateChanged(true);
    await handlerRemote.page!.$.flushForTesting();
    await microtasksFinished();
    assertTrue(element.$.actionButton.disabled);
  });
});
