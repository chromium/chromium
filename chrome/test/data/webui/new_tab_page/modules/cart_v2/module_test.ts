// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {CartHandlerRemote} from 'chrome://new-tab-page/chrome_cart.mojom-webui.js';
import {ChromeCartProxy, chromeCartV2Descriptor, ChromeCartV2ModuleElement, ModuleHeight} from 'chrome://new-tab-page/lazy_load.js';
import {$$, CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {fakeMetricsPrivate, MetricsTracker} from '../../../metrics_test_support.js';
import {assertNotStyle, installMock} from '../../test_support.js';

suite('NewTabPageModulesChromeCartModuleTest', () => {
  let handler: TestBrowserProxy<CartHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = installMock(CartHandlerRemote, ChromeCartProxy.setHandler);
    metrics = fakeMetricsPrivate();
    // Not show welcome surface by default.
    handler.setResultFor(
        'getWarmWelcomeVisible', Promise.resolve({welcomeVisible: false}));
    // Not show consent card by default.
    handler.setResultFor(
        'getDiscountConsentCardVisible',
        Promise.resolve({consentVisible: false}));
  });

  suite('Normal cart without rule-based discount', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({ruleBasedDiscountEnabled: false});
    });

    test('creates empty module if no cart item', async () => {
      // Arrange.
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts: []}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;

      // Assert.
      assertEquals(1, handler.getCallCount('getMerchantCarts'));
      assertTrue(!!moduleElement);
    });

    test('creates filled module if cart item', async () => {
      const carts = [
        {
          merchant: 'Amazon',
          cartUrl: {url: 'https://amazon.com'},
          productImageUrls: [
            {url: 'https://image1.com'},
            {url: 'https://image2.com'},
            {url: 'https://image3.com'},
          ],
        },
        {
          merchant: 'eBay',
          cartUrl: {url: 'https://ebay.com'},
          productImageUrls:
              [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
        },
        {
          merchant: 'BestBuy',
          cartUrl: {url: 'https://bestbuy.com'},
          imagproductImageUrlseUrls: [],
        },
        {
          merchant: 'Walmart',
          cartUrl: {url: 'https://walmart.com'},
          productImageUrls: [
            {url: 'https://image6.com'},
            {url: 'https://image7.com'},
            {url: 'https://image8.com'},
            {url: 'https://image9.com'},
          ],
        },
        {
          merchant: 'Nike',
          cartUrl: {url: 'https://nike.com'},
          productImageUrls: [{url: 'https://image10.com'}],
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      loadTimeData.overrideValues({
        modulesCartItemCountSingular: '$1 item',
        modulesCartItemCountMultiple: '$1 items',
      });

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(5, cartItems.length);
      assertEquals(1, metrics.count('NewTabPage.Carts.CartCount', 5));

      assertEquals('https://amazon.com/', cartItems[0]!.href);
      assertEquals(
          'Amazon',
          cartItems[0]!.querySelector<HTMLElement>('.merchant')!.innerText);
      let itemCount =
          cartItems[0]!.querySelector<HTMLElement>('.item-count')!.innerText;
      assertEquals('3 items', itemCount);
      let thumbnailList = cartItems[0]!.querySelector('.thumbnail-list')!
                              .querySelectorAll<CrAutoImgElement>('img');
      assertEquals(3, thumbnailList.length);
      assertEquals('https://image1.com', thumbnailList[0]!.autoSrc);
      assertEquals('https://image2.com', thumbnailList[1]!.autoSrc);
      assertEquals('https://image3.com', thumbnailList[2]!.autoSrc);
      assertEquals(null, cartItems[0]!.querySelector('.thumbnail-fallback'));

      assertEquals('https://ebay.com/', cartItems[1]!.href);
      assertEquals(
          'eBay',
          cartItems[1]!.querySelector<HTMLElement>('.merchant')!.innerText);
      itemCount =
          cartItems[1]!.querySelector<HTMLElement>('.item-count')!.innerText;
      assertEquals('2 items', itemCount);
      thumbnailList = cartItems[1]!.querySelector('.thumbnail-list')!
                          .querySelectorAll<CrAutoImgElement>('img');
      assertEquals(2, thumbnailList.length);
      assertEquals('https://image4.com', thumbnailList[0]!.autoSrc);
      assertEquals('https://image5.com', thumbnailList[1]!.autoSrc);
      assertEquals(null, cartItems[1]!.querySelector('.thumbnail-fallback'));

      assertEquals('https://bestbuy.com/', cartItems[2]!.href);
      assertEquals(
          'BestBuy',
          cartItems[2]!.querySelector<HTMLElement>('.merchant')!.innerText);
      assertEquals(
          null, cartItems[2]!.querySelector<HTMLElement>('.item-count')!);
      assertEquals(null, cartItems[2]!.querySelector('.thumbnail-list'));
      assertEquals(
          'chrome://new-tab-page/modules/cart/icons/cart_fallback.svg',
          cartItems[2]!.querySelector<HTMLImageElement>(
                           '.thumbnail-fallback')!.src);

      assertEquals('https://walmart.com/', cartItems[3]!.href);
      assertEquals(
          'Walmart',
          cartItems[3]!.querySelector<HTMLElement>('.merchant')!.innerText);
      itemCount =
          cartItems[3]!.querySelector<HTMLElement>('.item-count')!.innerText;
      assertEquals('4 items', itemCount);
      thumbnailList = cartItems[3]!.querySelector('.thumbnail-list')!
                          .querySelectorAll<CrAutoImgElement>('img');
      assertEquals(3, thumbnailList.length);
      assertEquals('https://image6.com', thumbnailList[0]!.autoSrc);
      assertEquals('https://image7.com', thumbnailList[1]!.autoSrc);
      assertEquals('https://image8.com', thumbnailList[2]!.autoSrc);
      assertEquals(null, cartItems[1]!.querySelector('.thumbnail-fallback'));

      assertEquals('https://nike.com/', cartItems[4]!.href);
      assertEquals(
          'Nike',
          cartItems[4]!.querySelector<HTMLElement>('.merchant')!.innerText);
      itemCount =
          cartItems[4]!.querySelector<HTMLElement>('.item-count')!.innerText;
      assertEquals('1 item', itemCount);
      thumbnailList = cartItems[4]!.querySelector('.thumbnail-list')!
                          .querySelectorAll<CrAutoImgElement>('img');
      assertEquals(1, thumbnailList.length);
      assertEquals('https://image10.com', thumbnailList[0]!.autoSrc);
      assertEquals(null, cartItems[4]!.querySelector('.thumbnail-fallback'));
    });

    test('dismiss and undo single cart item in module', async () => {
      // Arrange.
      const carts = [
        {
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
        },
        {
          merchant: 'Bar',
          cartUrl: {url: 'https://bar.com'},
          productImageUrls: [],
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(2, cartItems.length);
      let menuButton =
          cartItems[0]!.querySelector<HTMLElement>('.icon-more-vert')!;
      const actionMenu = moduleElement.$.cartActionMenu;
      assertFalse(actionMenu.open);

      // Act
      menuButton.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, handler.getCallCount('hideCart'));

      // Act
      moduleElement.$.hideCartButton.click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('hideCart'));
      assertTrue(moduleElement.$.dismissCartToast.open);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuHideMerchantToastMessage', 'Foo'),
          moduleElement.$.dismissCartToastMessage.textContent!.trim());
      assertNotStyle(moduleElement.$.undoDismissCartButton, 'display', 'none');
      assertEquals(0, handler.getCallCount('restoreHiddenCart'));

      // Act.
      moduleElement.$.undoDismissCartButton.click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('restoreHiddenCart'));
      assertFalse(actionMenu.open);

      // Act.
      menuButton = cartItems[1]!.querySelector<HTMLElement>('.icon-more-vert')!;
      menuButton.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, handler.getCallCount('removeCart'));

      // Act
      moduleElement.$.removeCartButton.click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('removeCart'));
      assertTrue(moduleElement.$.dismissCartToast.open);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuRemoveMerchantToastMessage', 'Bar'),
          moduleElement.$.dismissCartToastMessage.textContent!.trim());
      assertNotStyle(moduleElement.$.undoDismissCartButton, 'display', 'none');
      assertEquals(0, handler.getCallCount('restoreRemovedCart'));

      // Act
      moduleElement.$.undoDismissCartButton.click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('restoreRemovedCart'));
      assertFalse(actionMenu.open);
    });

    test('dismiss the last cart would hide the module', async () => {
      // Arrange.
      const data = [
        {
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
        },
      ];
      let carts = data;
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      let cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(1, cartItems.length);
      const actionMenu = moduleElement.$.cartActionMenu;
      assertFalse(actionMenu.open);

      // Act.
      cartItems[0]!.querySelector<HTMLElement>('.icon-more-vert')!.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, handler.getCallCount('hideCart'));

      // Act.
      carts = [];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      const waitForDismissEvent =
          eventToPromise('dismiss-module', moduleElement);
      moduleElement.$.hideCartButton.click();
      const hideEvent = await waitForDismissEvent;
      const hideToastMessage = hideEvent.detail.message;
      const hideRestoreCallback = hideEvent.detail.restoreCallback;
      cartItems = moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
          '.cart-item');

      // Assert.
      assertFalse(moduleElement.$.dismissCartToast.open);
      assertEquals(1, handler.getCallCount('hideCart'));
      assertEquals(0, cartItems.length);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuHideMerchantToastMessage', 'Foo'),
          hideToastMessage);
      assertEquals(
          1, metrics.count('NewTabPage.Carts.DismissLastCartHidesModule'));

      // Act.
      carts = data;
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      hideRestoreCallback();
      await flushTasks();
      cartItems = moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
          '.cart-item');

      // Assert.
      assertEquals(1, handler.getCallCount('restoreHiddenCart'));
      assertEquals(1, cartItems.length);
      assertEquals(
          1, metrics.count('NewTabPage.Carts.RestoreLastCartRestoresModule'));
    });

    test('scroll through module with full columns of 3 carts', async () => {
      // Arrange.
      const dummyMerchant = {
        merchant: 'Dummy',
        cartUrl: {url: 'https://dummy.com'},
        productImageUrls: [],
      };
      const carts = Array.from({length: 9}, () => dummyMerchant);
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      moduleElement.style.height = `${ModuleHeight.TALL}px`;
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel = moduleElement.$.cartCarousel;
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(9, cartItems.length);

      // Act.
      let waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      await waitForLeftScrollEnableChange;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 2);

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-show', moduleElement);
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.rightScrollButton.click();
      await waitForScrollFinished;
      await waitForLeftScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, false);
      checkVisibleRange(moduleElement, 3, 5);
      assertEquals(1, metrics.count('NewTabPage.Carts.RightScrollClick'));

      // Act.
      waitForRightScrollEnableChange =
          eventToPromise('right-scroll-hide', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.rightScrollButton.click();
      await waitForScrollFinished;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, true);
      checkVisibleRange(moduleElement, 6, 8);
      assertEquals(2, metrics.count('NewTabPage.Carts.RightScrollClick'));

      // Act.
      waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.leftScrollButton.click();
      await waitForScrollFinished;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, false);
      checkVisibleRange(moduleElement, 3, 5);
      assertEquals(1, metrics.count('NewTabPage.Carts.LeftScrollClick'));

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.leftScrollButton.click();
      await waitForScrollFinished;
      await waitForLeftScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 2);
      assertEquals(2, metrics.count('NewTabPage.Carts.LeftScrollClick'));

      // Remove the observer.
      cartCarousel.removeEventListener('scroll', onScroll);
    });

    test('scroll through module ending with a column of 2 carts', async () => {
      // Arrange.
      const dummyMerchant = {
        merchant: 'Dummy',
        cartUrl: {url: 'https://dummy.com'},
        productImageUrls: [],
      };
      const carts = Array.from({length: 5}, () => dummyMerchant);
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      moduleElement.style.height = `${ModuleHeight.TALL}px`;
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel = moduleElement.$.cartCarousel;
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(5, cartItems.length);

      // Act.
      let waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      await waitForLeftScrollEnableChange;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 2);

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-show', moduleElement);
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.rightScrollButton.click();
      await waitForLeftScrollEnableChange;
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, true);
      checkVisibleRange(moduleElement, 3, 4);

      // Act.
      waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.leftScrollButton.click();
      await waitForScrollFinished;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 2);

      // Remove the observer.
      cartCarousel.removeEventListener('scroll', onScroll);
    });

    test('scroll through module ending with a column of 1 cart', async () => {
      // Arrange.
      const dummyMerchant = {
        merchant: 'Dummy',
        cartUrl: {url: 'https://dummy.com'},
        productImageUrls: [],
      };
      const carts = Array.from({length: 4}, () => dummyMerchant);
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      moduleElement.style.height = `${ModuleHeight.TALL}px`;
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel = moduleElement.$.cartCarousel;
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(4, cartItems.length);

      // Act.
      let waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      await waitForLeftScrollEnableChange;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 2);

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-show', moduleElement);
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.rightScrollButton.click();
      await waitForLeftScrollEnableChange;
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, true);
      checkVisibleRange(moduleElement, 3, 3);

      // Act.
      waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.leftScrollButton.click();
      await waitForScrollFinished;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 2);

      // Remove the observer.
      cartCarousel.removeEventListener('scroll', onScroll);
    });

    test('shows discount chip', async () => {
      const carts = [
        {
          merchant: 'Amazon',
          cartUrl: {url: 'https://amazon.com'},
          productImageUrls: [],
          discountText: '15% off',
        },
        {
          merchant: 'eBay',
          cartUrl: {url: 'https://ebay.com'},
          productImageUrls: [],
          discountText: '50$ off',
        },
        {
          merchant: 'BestBuy',
          cartUrl: {url: 'https://bestbuy.com'},
          productImageUrls: [],
          discountText: '',
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(3, cartItems.length);

      assertEquals('https://amazon.com/', cartItems[0]!.href);
      assertEquals(
          '15% off',
          cartItems[0]!.querySelector<HTMLElement>(
                           '.discount-chip')!.innerText);
      assertEquals('https://ebay.com/', cartItems[1]!.href);
      assertEquals(
          '50$ off',
          cartItems[1]!.querySelector<HTMLElement>(
                           '.discount-chip')!.innerText);
      assertEquals('https://bestbuy.com/', cartItems[2]!.href);
      assertEquals(
          null, cartItems[2]!.querySelector<HTMLElement>('.discount-chip'));
    });

    test('shows and hides discount consent in cart module', async () => {
      const carts = [
        {
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      handler.setResultFor(
          'getDiscountConsentCardVisible',
          Promise.resolve({consentVisible: true}));
      loadTimeData.overrideValues({
        modulesCartDiscountConsentRejectConfirmation: 'Reject confirmation!',
        modulesCartDiscountConsentAcceptConfirmation: 'Accept confirmation!',
      });

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.consentCardElement.render();

      // Assert.
      const consentCard = $$(moduleElement, '#consentCard')!;
      const consentToast = moduleElement.$.confirmDiscountConsentToast;
      assertEquals(true, isVisible(consentCard));
      assertEquals(false, consentToast.open);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentContent'),
          consentCard.querySelector<HTMLElement>('#consentContent')!.innerText);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentReject'),
          consentCard.querySelector<HTMLElement>('#cancelButton')!.innerText);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentAccept'),
          consentCard.querySelector<HTMLElement>('#actionButton')!.innerText);

      // Act.
      consentCard.querySelector<HTMLElement>('#cancelButton')!.click();
      await flushTasks();

      // Assert.
      assertEquals(false, isVisible(consentCard));
      assertEquals(true, consentToast.open);
      assertEquals(
          'Reject confirmation!',
          moduleElement.$.confirmDiscountConsentMessage.innerText);

      // Act.
      moduleElement.$.confirmDiscountConsentButton.click();

      // Assert.
      assertEquals(false, consentToast.open);
      assertEquals(false, isVisible(consentCard));

      // Act.
      moduleElement.showDiscountConsent = true;
      moduleElement.$.consentCardElement.render();

      // Assert.
      assertEquals(true, isVisible(consentCard));

      // Act.
      consentCard.querySelector<HTMLElement>('#actionButton')!.click();
      await flushTasks();

      // Assert.
      assertEquals(false, isVisible(consentCard));
      assertEquals(true, consentToast.open);
      assertEquals(
          'Accept confirmation!',
          moduleElement.$.confirmDiscountConsentMessage.innerText);

      // Act.
      moduleElement.$.confirmDiscountConsentButton.click();

      // Assert.
      assertEquals(false, consentToast.open);
      assertEquals(false, isVisible(consentCard));
    });

    test('scroll with consent card', async () => {
      // Arrange.
      const dummyMerchant = {
        merchant: 'Dummy',
        cartUrl: {url: 'https://dummy.com'},
        productImageUrls: [],
      };
      const carts = Array.from({length: 10}, () => dummyMerchant);
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      handler.setResultFor(
          'getDiscountConsentCardVisible',
          Promise.resolve({consentVisible: true}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      moduleElement.style.height = `${ModuleHeight.TALL}px`;
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel = moduleElement.$.cartCarousel;
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(10, cartItems.length);

      // Act.
      let waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      const waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      await waitForLeftScrollEnableChange;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 0);

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-show', moduleElement);
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.rightScrollButton.click();
      await waitForLeftScrollEnableChange;
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, false);
      checkVisibleRange(moduleElement, 1, 3);

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.$.leftScrollButton.click();
      await waitForScrollFinished;
      await waitForLeftScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 0);

      // Remove the observer.
      cartCarousel.removeEventListener('scroll', onScroll);
    });

    test('click on cart item', async () => {
      const carts = [
        {
          merchant: 'Amazon',
          cartUrl: {url: 'https://amazon.com'},
          productImageUrls: [
            {url: 'https://image1.com'},
            {url: 'https://image2.com'},
            {url: 'https://image3.com'},
          ],
        },
        {
          merchant: 'eBay',
          cartUrl: {url: 'https://ebay.com'},
          productImageUrls:
              [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
        },
        {
          merchant: 'BestBuy',
          cartUrl: {url: 'https://bestbuy.com'},
          productImageUrls: [],
        },
        {
          merchant: 'Walmart',
          cartUrl: {url: 'https://walmart.com'},
          productImageUrls: [],
          discountText: '15% off',
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(4, cartItems.length);

      // Act.
      cartItems[3]!.querySelector<HTMLElement>('.cart-title')!.click();
      cartItems[0]!.click();
      cartItems[2]!.querySelector<HTMLElement>('.icon-more-vert')!.click();
      cartItems[1]!.querySelector<HTMLElement>('.thumbnail-container')!.click();

      // Assert.
      Array(4).forEach(
          index => assertEquals(
              1, metrics.count('NewTabPage.Carts.ClickCart', index)));
      assertEquals(0, handler.getCallCount('getDiscountURL'));
    });

    function checkScrollButtonDisabled(
        moduleElement: ChromeCartV2ModuleElement, isLeftDisabled: boolean,
        isRightDisabled: boolean) {
      assertEquals(isLeftDisabled, moduleElement.$.leftScrollButton.disabled);
      assertEquals(isRightDisabled, moduleElement.$.rightScrollButton.disabled);
    }

    function checkVisibleRange(
        moduleElement: ChromeCartV2ModuleElement, startIndex: number,
        endIndex: number) {
      const carts =
          moduleElement.$.cartCarousel.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertTrue(startIndex >= 0);
      assertTrue(endIndex < carts.length);
      for (let i = 0; i < carts.length; i++) {
        if (i >= startIndex && i <= endIndex) {
          assertTrue(getVisibilityForIndex(moduleElement, i));
        } else {
          assertFalse(getVisibilityForIndex(moduleElement, i));
        }
      }
    }

    function getVisibilityForIndex(
        moduleElement: ChromeCartV2ModuleElement, index: number): boolean {
      const cartCarousel = moduleElement.$.cartCarousel;
      const cart =
          cartCarousel.querySelectorAll<HTMLAnchorElement>('.cart-item')[index];
      return cart !== undefined &&
          (cart.offsetLeft === cartCarousel.scrollLeft) &&
          (cartCarousel.clientWidth <= cart.offsetWidth);
    }
  });

  suite('rule-based discount', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({ruleBasedDiscountEnabled: true});
    });

    test('click on cart item with rule-based discount', async () => {
      const carts = [
        {
          merchant: 'Amazon',
          cartUrl: {url: 'https://amazon.com'},
          productImageUrls: [
            {url: 'https://image1.com'},
            {url: 'https://image2.com'},
            {url: 'https://image3.com'},
          ],
        },
        {
          merchant: 'eBay',
          cartUrl: {url: 'https://ebay.com'},
          productImageUrls:
              [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
        },
        {
          merchant: 'BestBuy',
          cartUrl: {url: 'https://bestbuy.com'},
          productImageUrls: [],
        },
        {
          merchant: 'Walmart',
          cartUrl: {url: 'https://walmart.com'},
          productImageUrls: [],
          discountText: '15% off',
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      handler.setResultFor(
          'getDiscountURL',
          Promise.resolve({discountUrl: {url: 'https://www.foo.com'}}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize(0) as
          ChromeCartV2ModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(4, cartItems.length);

      // Act and assert.
      await testRBDCartClick(
          cartItems[3]!.querySelector<HTMLElement>('.discount-chip')!,
          'https://walmart.com', 3, 1, moduleElement);
      await testRBDCartClick(
          cartItems[0]!, 'https://amazon.com', 0, 2, moduleElement);
      await testRBDCartClick(
          cartItems[2]!, 'https://bestbuy.com', 2, 3, moduleElement);
      await testRBDCartClick(
          cartItems[1]!.querySelector('.thumbnail-container')!,
          'https://ebay.com', 1, 4, moduleElement);

      // Act.
      cartItems[0]!.querySelector<HTMLElement>('.icon-more-vert')!.click();

      // Assert.
      assertEquals(4, handler.getCallCount('getDiscountURL'));
    });

    async function testRBDCartClick(
        clickingElement: HTMLElement, cartURL: string, index: number,
        expectedCallCount: number, moduleElement: ChromeCartV2ModuleElement) {
      // Act.
      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      clickingElement.click();

      // Assert.
      assertEquals(0, metrics.count('NewTabPage.Carts.ClickCart', index));
      assertEquals(expectedCallCount, handler.getCallCount('getDiscountURL'));
      assertEquals(
          cartURL,
          handler.getArgs('getDiscountURL')[expectedCallCount - 1]!.url);

      // Act.
      // Wait for the following click event triggered by the first click to
      // propagate.
      await waitForUsageEvent;

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Carts.ClickCart', index));
      assertEquals(
          'https://www.foo.com/',
          moduleElement.shadowRoot!
              .querySelectorAll<HTMLAnchorElement>('.cart-item')[index]!.href);
    }
  });
});
