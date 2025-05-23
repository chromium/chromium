// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://extensions-zero-state/zero_state_promo_app.js';

import {WebStoreLinkClicked} from 'chrome://extensions-zero-state/zero_state_promo.mojom-webui.js';
import type {ZeroStatePromoAppElement} from 'chrome://extensions-zero-state/zero_state_promo_app.js';
import type {ZeroStatePromoBrowserProxy} from 'chrome://extensions-zero-state/zero_state_promo_browser_proxy.js';
import {ZeroStatePromoBrowserProxyImpl} from 'chrome://extensions-zero-state/zero_state_promo_browser_proxy.js';
import {CustomHelpBubbleUserAction} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import type {CustomHelpBubbleHandlerInterface} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import type {CustomHelpBubbleProxy} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble_proxy.js';
import {CustomHelpBubbleProxyImpl} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBubbleHandler extends TestBrowserProxy implements
    CustomHelpBubbleHandlerInterface {
  constructor() {
    super(['notifyUserAction']);
  }

  notifyUserAction(action: CustomHelpBubbleUserAction) {
    this.methodCalled('notifyUserAction', action);
  }
}

export class TestBubbleProxy implements CustomHelpBubbleProxy {
  private handler_: TestBubbleHandler;

  constructor(handler: TestBubbleHandler) {
    this.handler_ = handler;
  }

  getHandler(): TestBubbleHandler {
    return this.handler_;
  }
}

export class TestPromoProxy extends TestBrowserProxy implements
    ZeroStatePromoBrowserProxy {
  constructor() {
    super(['launchWebStoreLink']);
  }

  launchWebStoreLink(link: WebStoreLinkClicked) {
    this.methodCalled('launchWebStoreLink', link);
  }
}

suite('ChipsUiTest', () => {
  let bubbleHandler: TestBubbleHandler;
  let zeroStatePromoApp: ZeroStatePromoAppElement;
  let promoProxy: TestPromoProxy;

  setup(() => {
    bubbleHandler = new TestBubbleHandler();
    CustomHelpBubbleProxyImpl.setInstance(new TestBubbleProxy(bubbleHandler));

    promoProxy = new TestPromoProxy();
    ZeroStatePromoBrowserProxyImpl.setInstance(promoProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    zeroStatePromoApp =
        document.createElement('extensions-zero-state-promo-app');
    document.body.appendChild(zeroStatePromoApp);
  });

  test('ClickCouponsChip', async () => {
    zeroStatePromoApp.$.couponsButton.click();

    assertEquals(
        WebStoreLinkClicked.kCoupon,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickWritingChip', async () => {
    zeroStatePromoApp.$.writingButton.click();

    assertEquals(
        WebStoreLinkClicked.kWriting,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickProductivityChip', async () => {
    zeroStatePromoApp.$.productivityButton.click();

    assertEquals(
        WebStoreLinkClicked.kProductivity,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickAiChip', async () => {
    zeroStatePromoApp.$.aiButton.click();

    assertEquals(
        WebStoreLinkClicked.kAi,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickDismissButton', async () => {
    zeroStatePromoApp.$.dismissButton.click();

    assertEquals(
        CustomHelpBubbleUserAction.kDismiss,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });
});
