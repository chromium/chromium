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
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';


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

suite('ChipsUiV1Test', () => {
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

  function queryAppSelector(selector: string): HTMLElement|null {
    return zeroStatePromoApp.shadowRoot.querySelector<HTMLElement>(selector);
  }

  test('UiElementVisibility', () => {
    assertTrue(isVisible(queryAppSelector('#dismissButton')));
    assertTrue(isVisible(queryAppSelector('#couponsButton')));
    assertTrue(isVisible(queryAppSelector('#writingButton')));
    assertTrue(isVisible(queryAppSelector('#productivityButton')));
    assertTrue(isVisible(queryAppSelector('#aiButton')));
    assertFalse(isVisible(queryAppSelector('#webStoreButton')));
    assertFalse(isVisible(queryAppSelector('#couponsLink')));
    assertFalse(isVisible(queryAppSelector('#writingLink')));
    assertFalse(isVisible(queryAppSelector('#productivityLink')));
    assertFalse(isVisible(queryAppSelector('#aiLink')));
    assertFalse(isVisible(queryAppSelector('#closeButton')));
    assertFalse(isVisible(queryAppSelector('#customActionButton')));
  });

  test('ClickCouponsChip', async () => {
    const chip = queryAppSelector('#couponsButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kCoupon,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickWritingChip', async () => {
    const chip = queryAppSelector('#writingButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kWriting,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickProductivityChip', async () => {
    const chip = queryAppSelector('#productivityButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kProductivity,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickAiChip', async () => {
    const chip = queryAppSelector('#aiButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kAi,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickDismissButton', async () => {
    const button = queryAppSelector('#dismissButton');
    assertTrue(!!button);
    button.click();

    assertEquals(
        CustomHelpBubbleUserAction.kDismiss,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });
});

suite('ChipsUiV2Test', () => {
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

  function queryAppSelector(selector: string): HTMLElement|null {
    return zeroStatePromoApp.shadowRoot.querySelector<HTMLElement>(selector);
  }

  test('UiElementVisibility', () => {
    assertTrue(isVisible(queryAppSelector('#dismissButton')));
    assertTrue(isVisible(queryAppSelector('#couponsButton')));
    assertTrue(isVisible(queryAppSelector('#productivityButton')));
    assertTrue(isVisible(queryAppSelector('#aiButton')));
    assertTrue(isVisible(queryAppSelector('#webStoreButton')));
    assertFalse(isVisible(queryAppSelector('#writingButton')));
    assertFalse(isVisible(queryAppSelector('#couponsLink')));
    assertFalse(isVisible(queryAppSelector('#writingLink')));
    assertFalse(isVisible(queryAppSelector('#productivityLink')));
    assertFalse(isVisible(queryAppSelector('#aiLink')));
    assertFalse(isVisible(queryAppSelector('#closeButton')));
    assertFalse(isVisible(queryAppSelector('#customActionButton')));
  });

  test('ClickCouponsChip', async () => {
    const chip = queryAppSelector('#couponsButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kCoupon,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickWebstoreChip', async () => {
    const chip = queryAppSelector('#webStoreButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kDiscoverExtension,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickProductivityChip', async () => {
    const chip = queryAppSelector('#productivityButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kProductivity,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickAiChip', async () => {
    const chip = queryAppSelector('#aiButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kAi,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickDismissButton', async () => {
    const button = queryAppSelector('#dismissButton');
    assertTrue(!!button);
    button.click();

    assertEquals(
        CustomHelpBubbleUserAction.kDismiss,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });
});

suite('ChipsUiV3Test', () => {
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

  function queryAppSelector(selector: string): HTMLElement|null {
    return zeroStatePromoApp.shadowRoot.querySelector<HTMLElement>(selector);
  }

  test('UiElementVisibility', () => {
    assertTrue(isVisible(queryAppSelector('#dismissButton')));
    assertTrue(isVisible(queryAppSelector('#couponsButton')));
    assertTrue(isVisible(queryAppSelector('#writingButton')));
    assertTrue(isVisible(queryAppSelector('#productivityButton')));
    assertTrue(isVisible(queryAppSelector('#aiButton')));
    assertFalse(isVisible(queryAppSelector('#webStoreButton')));
    assertFalse(isVisible(queryAppSelector('#couponsLink')));
    assertFalse(isVisible(queryAppSelector('#writingLink')));
    assertFalse(isVisible(queryAppSelector('#productivityLink')));
    assertFalse(isVisible(queryAppSelector('#aiLink')));
    assertFalse(isVisible(queryAppSelector('#closeButton')));
    assertFalse(isVisible(queryAppSelector('#customActionButton')));
  });

  test('ClickCouponsChip', async () => {
    const chip = queryAppSelector('#couponsButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kCoupon,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickWritingChip', async () => {
    const chip = queryAppSelector('#writingButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kWriting,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickProductivityChip', async () => {
    const chip = queryAppSelector('#productivityButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kProductivity,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickAiChip', async () => {
    const chip = queryAppSelector('#aiButton');
    assertTrue(!!chip);
    chip.click();

    assertEquals(
        WebStoreLinkClicked.kAi,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickDismissButton', async () => {
    const button = queryAppSelector('#dismissButton');
    assertTrue(!!button);
    button.click();

    assertEquals(
        CustomHelpBubbleUserAction.kDismiss,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });
});

suite('PlainLinkUiTest', () => {
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

  function queryAppSelector(selector: string): HTMLElement|null {
    return zeroStatePromoApp.shadowRoot.querySelector<HTMLElement>(selector);
  }

  test('UiElementVisibility', () => {
    assertTrue(isVisible(queryAppSelector('#dismissButton')));
    assertTrue(isVisible(queryAppSelector('#couponsLink')));
    assertTrue(isVisible(queryAppSelector('#writingLink')));
    assertTrue(isVisible(queryAppSelector('#productivityLink')));
    assertTrue(isVisible(queryAppSelector('#aiLink')));
    assertTrue(isVisible(queryAppSelector('#closeButton')));
    assertTrue(isVisible(queryAppSelector('#customActionButton')));
    assertFalse(isVisible(queryAppSelector('#couponsButton')));
    assertFalse(isVisible(queryAppSelector('#writingButton')));
    assertFalse(isVisible(queryAppSelector('#productivityButton')));
    assertFalse(isVisible(queryAppSelector('#aiButton')));
  });

  test('ClickCouponsLink', async () => {
    const link = queryAppSelector('#couponsLink');
    assertTrue(!!link);
    link.click();

    assertEquals(
        WebStoreLinkClicked.kCoupon,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickWritingLink', async () => {
    const link = queryAppSelector('#writingLink');
    assertTrue(!!link);
    link.click();

    assertEquals(
        WebStoreLinkClicked.kWriting,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickProductivityLink', async () => {
    const link = queryAppSelector('#productivityLink');
    assertTrue(!!link);
    link.click();

    assertEquals(
        WebStoreLinkClicked.kProductivity,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickAiLink', async () => {
    const link = queryAppSelector('#aiLink');
    assertTrue(!!link);
    link.click();

    assertEquals(
        WebStoreLinkClicked.kAi,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickCustomActionButton', async () => {
    const button = queryAppSelector('#customActionButton');
    assertTrue(!!button);
    button.click();

    assertEquals(
        WebStoreLinkClicked.kDiscoverExtension,
        await promoProxy.whenCalled('launchWebStoreLink'));
    assertEquals(
        CustomHelpBubbleUserAction.kAction,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickDismissButton', async () => {
    const button = queryAppSelector('#dismissButton');
    assertTrue(!!button);
    button.click();

    assertEquals(
        CustomHelpBubbleUserAction.kDismiss,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });

  test('ClickCloseButton', async () => {
    const button = queryAppSelector('#closeButton');
    assertTrue(!!button);
    button.click();

    assertEquals(
        CustomHelpBubbleUserAction.kDismiss,
        await bubbleHandler.whenCalled('notifyUserAction'));
  });
});
