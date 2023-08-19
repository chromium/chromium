// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CartHandlerRemote, ConsentStatus} from 'chrome://new-tab-page/chrome_cart.mojom-webui.js';
import {chromeCartDescriptor, ChromeCartModuleElement, ChromeCartProxy, DiscountConsentCard, DiscountConsentVariation} from 'chrome://new-tab-page/lazy_load.js';
import {$$, CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, installMock} from '../../test_support.js';

import {clickAcceptButton, clickCloseButton, clickRejectButton, nextStep} from './discount_consent_card_test_utils.js';

suite('NewTabPageModulesChromeCartModuleTest', () => {
  let handler: TestMock<CartHandlerRemote>;
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
      loadTimeData.overrideValues({
        ruleBasedDiscountEnabled: false,
        modulesCartDiscountConsentVariation: DiscountConsentVariation.DEFAULT,
      });
    });

    test('creates no module if no cart item', async () => {
      // Arrange.
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts: []}));

      // Act.
      const moduleElement = await chromeCartDescriptor.initialize(0);

      // Assert.
      assertEquals(1, handler.getCallCount('getMerchantCarts'));
      assertEquals(null, moduleElement);
    });

    test('creates module if cart item', async () => {
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
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Act.
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(4, cartItems.length);
      assertEquals(220, moduleElement.offsetHeight);
      assertEquals(1, metrics.count('NewTabPage.Carts.CartCount', 4));
      assertEquals(1, metrics.count('NewTabPage.Carts.CartImageCount', 3));
      assertEquals(1, metrics.count('NewTabPage.Carts.CartImageCount', 2));
      assertEquals(1, metrics.count('NewTabPage.Carts.CartImageCount', 0));
      assertEquals(1, metrics.count('NewTabPage.Carts.CartImageCount', 4));

      assertEquals('https://amazon.com/', cartItems[0]!.href);
      assertEquals(
          'Amazon',
          cartItems[0]!.querySelector<HTMLElement>('.merchant')!.innerText);
      let itemCount =
          cartItems[0]!.querySelector<HTMLElement>('.item-count')!.innerText;
      assertEquals('3', itemCount.slice(-1));
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
      assertEquals('2', itemCount.slice(-1));
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
          null, cartItems[2]!.querySelector<HTMLElement>('.item-count'));
      assertEquals(null, cartItems[2]!.querySelector('.thumbnail-list'));
      assertEquals(
          'chrome://new-tab-page/modules/cart/icons/cart_fallback.svg',
          cartItems[2]!
              .querySelector<HTMLImageElement>('.thumbnail-fallback-img')!.src);

      assertEquals('https://walmart.com/', cartItems[3]!.href);
      assertEquals(
          'Walmart',
          cartItems[3]!.querySelector<HTMLElement>('.merchant')!.innerText);
      itemCount =
          cartItems[3]!.querySelector<HTMLElement>('.item-count')!.innerText;
      assertEquals('4', itemCount.slice(-1));
      thumbnailList = cartItems[3]!.querySelector('.thumbnail-list')!
                          .querySelectorAll<CrAutoImgElement>('img');
      assertEquals(3, thumbnailList.length);
      assertEquals('https://image6.com', thumbnailList[0]!.autoSrc);
      assertEquals('https://image7.com', thumbnailList[1]!.autoSrc);
      assertEquals('https://image8.com', thumbnailList[2]!.autoSrc);
      assertEquals(null, cartItems[1]!.querySelector('.thumbnail-fallback'));
    });

    test('shows welcome surface in cart module', async () => {
      const carts = [
        {
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      handler.setResultFor(
          'getWarmWelcomeVisible', Promise.resolve({welcomeVisible: true}));

      // Arrange.
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const headerChip =
          moduleElement.shadowRoot!.querySelector('ntp-module-header')!
              .shadowRoot!.querySelector<HTMLElement>('#chip');
      const headerDescription =
          moduleElement.shadowRoot!.querySelector('ntp-module-header')!
              .shadowRoot!.querySelector<HTMLElement>('#description');
      assertEquals(
          loadTimeData.getString('modulesNewTagLabel'), headerChip!.innerText);
      assertEquals(
          loadTimeData.getString('modulesCartWarmWelcome'),
          headerDescription!.innerText);
      assertEquals(227, moduleElement.offsetHeight);
      assertEquals(0, metrics.count('NewTabPage.Carts.CartImageCount', 0));
    });

    test(
        'Backend is notified when module is dismissed or restored',
        async () => {
          // Arrange.
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
          ];
          handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
          loadTimeData.overrideValues({
            disableModuleToastMessage: 'hello $1',
            modulesCartLowerYour: 'world',
          });

          // Arrange.
          const moduleElement = await chromeCartDescriptor.initialize(0) as
              ChromeCartModuleElement;
          assertTrue(!!moduleElement);
          document.body.append(moduleElement);
          moduleElement.$.cartItemRepeat.render();

          // Act.
          const waitForDismissEvent =
              eventToPromise('dismiss-module', moduleElement);
          ($$(moduleElement, 'ntp-module-header')!
           ).dispatchEvent(new Event('dismiss-button-click', {bubbles: true}));
          const hideEvent = await waitForDismissEvent;
          const hideToastMessage = hideEvent.detail.message;
          const hideRestoreCallback = hideEvent.detail.restoreCallback;

          // Assert.
          assertEquals(
              loadTimeData.getString('modulesCartModuleMenuHideToastMessage'),
              hideToastMessage);
          assertEquals(1, handler.getCallCount('hideCartModule'));
          assertEquals(1, metrics.count('NewTabPage.Carts.HideModule'));

          // Act.
          hideRestoreCallback();

          // Assert.
          assertEquals(1, handler.getCallCount('restoreHiddenCartModule'));
          assertEquals(1, metrics.count('NewTabPage.Carts.UndoHideModule'));

          // Act.
          const waitForDisableEvent =
              eventToPromise('disable-module', moduleElement);
          ($$(moduleElement, 'ntp-module-header')!
           ).dispatchEvent(new Event('disable-button-click', {bubbles: true}));
          const disableEvent = await waitForDisableEvent;
          const disableToastMessage = disableEvent.detail.message;
          const disableRestoreCallback = disableEvent.detail.restoreCallback;

          // Assert.
          assertEquals('hello world', disableToastMessage);
          assertEquals(1, metrics.count('NewTabPage.Carts.RemoveModule'));

          // Act.
          disableRestoreCallback();

          // Assert.
          assertEquals(1, metrics.count('NewTabPage.Carts.UndoRemoveModule'));
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
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartContainers =
          moduleElement.shadowRoot!.querySelectorAll('.cart-container');
      assertEquals(2, cartContainers.length);
      let menuButton =
          cartContainers[0]!.querySelector<HTMLElement>('.icon-more-vert')!;
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
      assertTrue(!!moduleElement.$.dismissCartToast.open);
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
      menuButton =
          cartContainers[1]!.querySelector<HTMLElement>('.icon-more-vert')!;
      menuButton.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, handler.getCallCount('removeCart'));

      // Act
      moduleElement.$.removeCartButton.click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('removeCart'));
      assertTrue(!!moduleElement.$.dismissCartToast.open);
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
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      let cartContainers =
          moduleElement.shadowRoot!.querySelectorAll('.cart-container');
      assertEquals(1, cartContainers.length);
      const actionMenu = moduleElement.$.cartActionMenu;
      assertFalse(actionMenu.open);

      // Act.
      cartContainers[0]!.querySelector<HTMLElement>('.icon-more-vert')!.click();

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
      cartContainers =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');

      // Assert.
      assertFalse(moduleElement.$.dismissCartToast.open);
      assertEquals(1, handler.getCallCount('hideCart'));
      assertEquals(0, cartContainers.length);
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
      cartContainers =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');

      // Assert.
      assertEquals(1, handler.getCallCount('restoreHiddenCart'));
      assertEquals(1, cartContainers.length);
      assertEquals(
          1, metrics.count('NewTabPage.Carts.RestoreLastCartRestoresModule'));
    });

    test('scroll with full width module', async () => {
      // Arrange.
      const dummyMerchant = {
        merchant: 'Dummy',
        cartUrl: {url: 'https://dummy.com'},
        productImageUrls: [],
      };
      const carts = Array.from({length: 10}, () => dummyMerchant);
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
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
      let waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollVisibilityChange =
          eventToPromise('right-scroll-show', moduleElement);
      moduleElement.style.width = '560px';
      await waitForLeftScrollVisibilityChange;
      await waitForRightScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, false, true);
      checkVisibleRange(moduleElement, 0, 3);

      // Act.
      waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-show', moduleElement);
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot!
          .querySelector<HTMLElement>('#rightScrollButton')!.click();
      await waitForScrollFinished;
      await waitForLeftScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, true, true);
      checkVisibleRange(moduleElement, 4, 7);
      assertEquals(1, metrics.count('NewTabPage.Carts.RightScrollClick'));

      // Act.
      waitForRightScrollVisibilityChange =
          eventToPromise('right-scroll-hide', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot!
          .querySelector<HTMLElement>('#rightScrollButton')!.click();
      await waitForScrollFinished;
      await waitForRightScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, true, false);
      checkVisibleRange(moduleElement, 6, 9);
      assertEquals(2, metrics.count('NewTabPage.Carts.RightScrollClick'));

      // Act.
      waitForRightScrollVisibilityChange =
          eventToPromise('right-scroll-show', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#leftScrollButton')!.click();
      await waitForScrollFinished;
      await waitForRightScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, true, true);
      checkVisibleRange(moduleElement, 2, 5);
      assertEquals(1, metrics.count('NewTabPage.Carts.LeftScrollClick'));

      // Act.
      waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-hide', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#leftScrollButton')!.click();
      await waitForScrollFinished;
      await waitForLeftScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, false, true);
      checkVisibleRange(moduleElement, 0, 3);
      assertEquals(2, metrics.count('NewTabPage.Carts.LeftScrollClick'));

      // Remove the observer.
      cartCarousel.removeEventListener('scroll', onScroll);
    });

    test('scroll with cutted width module', async () => {
      // Arrange.
      const dummyMerchant = {
        merchant: 'Dummy',
        cartUrl: {url: 'https://dummy.com'},
        productImageUrls: [],
      };
      const carts = Array.from({length: 10}, () => dummyMerchant);
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
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
      let waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-hide', moduleElement);
      const waitForRightScrollVisibilityChange =
          eventToPromise('right-scroll-show', moduleElement);
      moduleElement.style.width = '480px';
      await waitForLeftScrollVisibilityChange;
      await waitForRightScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, false, true);
      checkVisibleRange(moduleElement, 0, 2);

      // Act.
      waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-show', moduleElement);
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot!
          .querySelector<HTMLElement>('#rightScrollButton')!.click();
      await waitForLeftScrollVisibilityChange;
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonVisibility(moduleElement, true, true);
      checkVisibleRange(moduleElement, 3, 5);

      // Act.
      moduleElement.style.width = '220px';

      // Assert.
      checkScrollButtonVisibility(moduleElement, true, true);
      checkVisibleRange(moduleElement, 3, 3);

      // Act.
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot!
          .querySelector<HTMLElement>('#rightScrollButton')!.click();
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonVisibility(moduleElement, true, true);
      checkVisibleRange(moduleElement, 4, 4);

      // Remove the observer.
      cartCarousel.removeEventListener('scroll', onScroll);
    });

    test('scroll button visibility changes with module width', async () => {
      // Arrange.
      const dummyMerchant = {
        merchant: 'Dummy',
        cartUrl: {url: 'https://dummy.com'},
        productImageUrls: [],
      };
      const carts = Array.from({length: 4}, () => dummyMerchant);
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems =
          moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
              '.cart-item');
      assertEquals(4, cartItems.length);

      // Act.
      const waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollVisibilityChange =
          eventToPromise('right-scroll-hide', moduleElement);
      moduleElement.style.width = '560px';
      await waitForLeftScrollVisibilityChange;
      await waitForRightScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, false, false);
      checkVisibleRange(moduleElement, 0, 3);

      // Act.
      waitForRightScrollVisibilityChange =
          eventToPromise('right-scroll-show', moduleElement);
      moduleElement.style.width = '480px';
      await waitForRightScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, false, true);
      checkVisibleRange(moduleElement, 0, 2);

      // Act.
      waitForRightScrollVisibilityChange =
          eventToPromise('right-scroll-hide', moduleElement);
      moduleElement.style.width = '560px';
      await waitForRightScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, false, false);
      checkVisibleRange(moduleElement, 0, 3);
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
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
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
      assertEquals(null, cartItems[2]!.querySelector('.discount-chip'));
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
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement, 'Module should exist');
      document.body.append(moduleElement);
      moduleElement.$.consentCardElement.render();
      let transitionend =
          eventToPromise('transitionend', moduleElement.$.consentContainer);

      // Assert.
      const consentCard = $$<HTMLElement>(moduleElement, '#consentCard')!;
      const consentToast = moduleElement.$.confirmDiscountConsentToast;
      assertEquals(
          true, isVisible(consentCard), 'Consent card should be visible');
      assertEquals(
          false, consentToast.open, 'Consent toast should not be opened');
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentContent'),
          consentCard.querySelector<HTMLElement>('#consentContent')!.innerText);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentReject'),
          consentCard.querySelector<HTMLElement>('#cancelButton')!.innerText);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentAccept'),
          consentCard.querySelector<HTMLElement>('#actionButton')!.innerText);
      assertEquals(0, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));

      const consentCardV2 = $$<HTMLElement>(moduleElement, '#consentCardV2');
      assertTrue(consentCardV2 === null);

      // Act.
      consentCard.querySelector<HTMLElement>('#cancelButton')!.click();
      await flushTasks();
      await transitionend;

      // Assert.
      assertEquals(
          false, isVisible(consentCard),
          'Consent card should not be visible after clicking the cancel button');
      assertEquals(
          true, consentToast.open,
          'Consent toast should be opened after clicking the cancel button');
      assertEquals(
          'Reject confirmation!',
          moduleElement.$.confirmDiscountConsentMessage.innerText);
      assertEquals(1, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));

      // Act.
      moduleElement.$.confirmDiscountConsentButton.click();

      // Assert.
      assertEquals(
          false, consentToast.open,
          'Consent toast should not be opened after acting on the toast');
      assertEquals(
          false, isVisible(consentCard),
          'Consent card should not be visible after acting on the toast');

      // Act.
      moduleElement.showDiscountConsent = true;
      moduleElement.$.consentCardElement.render();
      assertEquals(true, isVisible(consentCard));
      transitionend =
          eventToPromise('transitionend', moduleElement.$.consentContainer);

      // Assert.
      assertEquals(
          true, isVisible(consentCard), 'Consent card should be visible1');
      assertEquals(0, metrics.count('NewTabPage.Carts.AcceptDiscountConsent'));

      // Act.
      consentCard.querySelector<HTMLElement>('#actionButton')!.click();
      await flushTasks();
      await transitionend;

      // Assert.
      assertEquals(
          false, isVisible(consentCard),
          'Consent card should not be visible after accepting');
      assertEquals(
          true, consentToast.open,
          'Consent toast should be opened after accepting');
      assertEquals(
          'Accept confirmation!',
          moduleElement.$.confirmDiscountConsentMessage.innerText);

      // Act.
      moduleElement.$.confirmDiscountConsentButton.click();

      // Assert.
      assertEquals(
          false, consentToast.open,
          'Consent toast should not be opened after acting on confirm toast');
      assertEquals(
          false, isVisible(consentCard),
          'Consent card should not be visible after confirm toast');
      assertEquals(1, metrics.count('NewTabPage.Carts.AcceptDiscountConsent'));
    });

    test('scroll with consent card', async () => testScrollWithConsent());

    // https://crbug.com/1287294: Flaky
    test.skip('click on cart item', async () => {
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
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
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
      cartItems[2]!.querySelector<HTMLElement>('.favicon-image')!.click();
      cartItems[1]!.querySelector<HTMLElement>('.thumbnail-container')!.click();

      // Assert.
      assertEquals(4, handler.getCallCount('prepareForNavigation'));
      for (let index = 0; index < 4; index++) {
        assertEquals(1, metrics.count('NewTabPage.Carts.ClickCart', index));
        assertEquals(true, handler.getArgs('prepareForNavigation')[index][1]);
      }
      assertEquals(0, handler.getCallCount('getDiscountURL'));
    });
  });


  async function testScrollWithConsent() {
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
    const moduleElement =
        await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
    assertTrue(!!moduleElement);
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
    let waitForLeftScrollVisibilityChange =
        eventToPromise('left-scroll-hide', moduleElement);
    const waitForRightScrollVisibilityChange =
        eventToPromise('right-scroll-show', moduleElement);
    moduleElement.style.width = '560px';
    await waitForLeftScrollVisibilityChange;
    await waitForRightScrollVisibilityChange;

    // Assert.
    checkScrollButtonVisibility(moduleElement, false, true);
    checkVisibleRange(moduleElement, 0, 1);

    // Act.
    waitForLeftScrollVisibilityChange =
        eventToPromise('left-scroll-show', moduleElement);
    let waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
    moduleElement.shadowRoot!.querySelector<HTMLElement>(
                                 '#rightScrollButton')!.click();
    await waitForScrollFinished;
    await waitForLeftScrollVisibilityChange;

    // Assert.
    checkScrollButtonVisibility(moduleElement, true, true);
    checkVisibleRange(moduleElement, 2, 5);

    // Act.
    waitForLeftScrollVisibilityChange =
        eventToPromise('left-scroll-hide', moduleElement);
    waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
    moduleElement.shadowRoot!.querySelector<HTMLElement>(
                                 '#leftScrollButton')!.click();
    await waitForScrollFinished;
    await waitForLeftScrollVisibilityChange;

    // Assert.
    checkScrollButtonVisibility(moduleElement, false, true);
    checkVisibleRange(moduleElement, 0, 1);
  }

  function checkScrollButtonVisibility(
      moduleElement: ChromeCartModuleElement, isLeftVisible: boolean,
      isRightVisible: boolean) {
    assertEquals(
        isLeftVisible,
        isVisible(
            moduleElement.shadowRoot!.querySelector('#leftScrollShadow')));
    assertEquals(
        isLeftVisible,
        isVisible(
            moduleElement.shadowRoot!.querySelector('#leftScrollButton')));
    assertEquals(
        isRightVisible,
        isVisible(
            moduleElement.shadowRoot!.querySelector('#rightScrollShadow')));
    assertEquals(
        isRightVisible,
        isVisible(
            moduleElement.shadowRoot!.querySelector('#rightScrollButton')));
  }

  function checkVisibleRange(
      moduleElement: ChromeCartModuleElement, startIndex: number,
      endIndex: number) {
    const carts =
        moduleElement.$.cartCarousel.querySelectorAll('.cart-container');
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
      moduleElement: ChromeCartModuleElement, index: number) {
    const cartCarousel = moduleElement.$.cartCarousel;
    const cart =
        cartCarousel.querySelectorAll<HTMLElement>('.cart-container')[index]!;
    return (cart.offsetLeft > cartCarousel.scrollLeft) &&
        (cartCarousel.scrollLeft + cartCarousel.clientWidth) >
        (cart.offsetLeft + cart.offsetWidth);
  }

  suite('rule-based discount', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({ruleBasedDiscountEnabled: true});
    });

    // https://crbug.com/1287294: Flaky
    test.skip('click on cart item with rule-based discount', async () => {
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
      const moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
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
          cartItems[3]!.querySelector('.discount-chip')!, 'https://walmart.com',
          3, 1, moduleElement);
      await testRBDCartClick(
          cartItems[0]!, 'https://amazon.com', 0, 2, moduleElement);
      await testRBDCartClick(
          cartItems[2]!.querySelector('.favicon-image')!, 'https://bestbuy.com',
          2, 3, moduleElement);
      await testRBDCartClick(
          cartItems[1]!.querySelector('.thumbnail-container')!,
          'https://ebay.com', 1, 4, moduleElement);

      // Act.
      const cartContainers =
          moduleElement.shadowRoot!.querySelectorAll('.cart-container');
      cartContainers[0]!.querySelector<HTMLElement>('.icon-more-vert')!.click();

      // Assert.
      assertEquals(4, handler.getCallCount('getDiscountURL'));
    });

    async function testRBDCartClick(
        clickingElement: HTMLElement, cartURL: string, index: number,
        expectedCallCount: number, moduleElement: ChromeCartModuleElement) {
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

    test('record discount consent show', async () => {
      // Arrange.
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

      assertEquals(0, metrics.count('NewTabPage.Carts.DiscountConsentShow', 1));

      // Act.
      await chromeCartDescriptor.initialize(0);

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Carts.DiscountConsentShow', 1));
    });

    test('record discount carts count and index', async () => {
      // Arrange.
      const carts = [
        {
          merchant: 'Boo',
          cartUrl: {url: 'https://Boo.com'},
          productImageUrls: [],
          discountText: '5% off',
        },
        {
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
        },
        {
          merchant: 'Koo',
          cartUrl: {url: 'https://Koo.com'},
          productImageUrls: [],
          discountText: '10% off',
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      assertEquals(0, metrics.count('NewTabPage.Carts.DiscountCountAtLoad', 2));
      assertEquals(
          0, metrics.count('NewTabPage.Carts.NonDiscountCountAtLoad', 1));
      assertEquals(0, metrics.count('NewTabPage.Carts.DiscountAt', 0));
      assertEquals(0, metrics.count('NewTabPage.Carts.DiscountAt', 2));
      assertEquals(0, metrics.count('NewTabPage.Carts.CartCount', 3));

      // Act.
      await chromeCartDescriptor.initialize(0);

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Carts.DiscountCountAtLoad', 2));
      assertEquals(
          1, metrics.count('NewTabPage.Carts.NonDiscountCountAtLoad', 1));
      assertEquals(1, metrics.count('NewTabPage.Carts.DiscountAt', 0));
      assertEquals(1, metrics.count('NewTabPage.Carts.DiscountAt', 2));
      assertEquals(1, metrics.count('NewTabPage.Carts.CartCount', 3));
    });
  });

  suite('Discount consent v2', () => {
    let moduleElement: ChromeCartModuleElement;

    setup(() => {
      handler.setResultFor(
          'getDiscountConsentCardVisible',
          Promise.resolve({consentVisible: true}));
      loadTimeData.overrideValues({
        modulesCartDiscountInlineCardShowCloseButton: true,
        modulesCartConsentStepTwoDifferentColor: false,
        modulesCartDiscountConsentRejectConfirmation: 'Reject confirmation!',
        modulesCartDiscountConsentAcceptConfirmation: 'Accept confirmation!',
        modulesCartDiscountConsentVariation: DiscountConsentVariation.INLINE,
        modulesCartStepOneUseStaticContent: true,
        modulesCartConsentStepOneButton: 'Continue',
        modulesCartStepOneStaticContent: 'Step one consent',
        modulesCartConsentStepTwoContent: 'Step two consent',
      });
    });

    async function testClickingAcceptButtonOnConsentCard() {
      // Arrange.
      const transitionend =
          eventToPromise('transitionend', moduleElement.$.consentContainer);
      const consentToast = moduleElement.$.confirmDiscountConsentToast;
      const consentCard =
          $$<DiscountConsentCard>(moduleElement, '#consentCardV2')!;
      assertEquals(0, metrics.count('NewTabPage.Carts.AcceptDiscountConsent'));
      assertEquals(true, isVisible(consentCard));
      nextStep(consentCard);
      await flushTasks();

      // Act.
      clickAcceptButton(consentCard);
      await flushTasks();
      await transitionend;

      // Assert.
      assertEquals(
          false, isVisible(consentCard), 'consent cart should not be visible');
      assertEquals(true, consentToast.open, 'consentToast should open');
      assertEquals(
          'Accept confirmation!',
          moduleElement.$.confirmDiscountConsentMessage.innerText);
      assertEquals(1, metrics.count('NewTabPage.Carts.AcceptDiscountConsent'));
    }

    test('shows discount consent v2 in cart module', async () => {
      const carts = [
        {
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
        },
        {
          merchant: 'Boo',
          cartUrl: {url: 'https://Boo.com'},
          productImageUrls: [],
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      moduleElement =
          await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
      assertTrue(!!moduleElement);
      document.body.append(moduleElement);
      moduleElement.$.consentCardElement.render();

      // Assert.
      const consentCard =
          $$<DiscountConsentCard>(moduleElement, '#consentCardV2')!;
      assertEquals(true, isVisible(consentCard));
    });

    test('scroll with consent card v2', async () => testScrollWithConsent());

    suite('Inline consent with close button', () => {
      let consentCard: DiscountConsentCard;

      setup(async () => {
        loadTimeData.overrideValues({
          modulesCartDiscountInlineCardShowCloseButton: true,
        });

        const carts = [
          {
            merchant: 'Foo',
            cartUrl: {url: 'https://foo.com'},
            productImageUrls: [],
          },
          {
            merchant: 'Boo',
            cartUrl: {url: 'https://Boo.com'},
            productImageUrls: [],
          },
        ];
        handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
        moduleElement =
            await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
        assertTrue(!!moduleElement);
        document.body.append(moduleElement);
        moduleElement.$.consentCardElement.render();

        consentCard = $$<DiscountConsentCard>(moduleElement, '#consentCardV2')!;
      });

      test(
          'Verify clicking close button in step 1 hides consent card',
          async () => {
            // Arrange.
            assertEquals(true, isVisible(consentCard));
            const transitionend = eventToPromise(
                'transitionend', moduleElement.$.consentContainer);
            assertEquals(
                0, metrics.count('NewTabPage.Carts.DismissDiscountConsent'),
                'Dismissed count should be 0 before clicking');
            // Act.
            clickCloseButton(consentCard);
            await flushTasks();
            await transitionend;

            // Assert.
            assertEquals(false, isVisible(consentCard));
            assertEquals(1, handler.getCallCount('onDiscountConsentDismissed'));
            assertEquals(
                1, metrics.count('NewTabPage.Carts.DismissDiscountConsent'),
                'Dismissed count should be 1 after clicking');
          });

      test(
          'Verify clicking close button in step 2 hides consent card',
          async () => {
            // Arrange.
            assertEquals(true, isVisible(consentCard));
            const transitionend = eventToPromise(
                'transitionend', moduleElement.$.consentContainer);
            const consentToast = moduleElement.$.confirmDiscountConsentToast;
            assertEquals(
                0, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));
            nextStep(consentCard);
            await flushTasks();

            // Act.
            clickCloseButton(consentCard);
            await flushTasks();
            await transitionend;

            // Assert.
            assertEquals(false, isVisible(consentCard));
            assertEquals(true, consentToast.open);
            assertEquals(
                'Reject confirmation!',
                moduleElement.$.confirmDiscountConsentMessage.innerText);
            assertEquals(
                1, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));
            assertEquals(0, handler.getCallCount('onDiscountConsentDismissed'));
          });

      test(
          'Verify clicking accept button in step 2 hides consent card',
          async () => testClickingAcceptButtonOnConsentCard());
    });

    suite('Inline consent with no close button', () => {
      let consentCard: DiscountConsentCard;

      setup(async () => {
        loadTimeData.overrideValues(
            {modulesCartDiscountInlineCardShowCloseButton: false});

        const carts = [
          {
            merchant: 'Foo',
            cartUrl: {url: 'https://foo.com'},
            productImageUrls: [],
          },
          {
            merchant: 'Boo',
            cartUrl: {url: 'https://Boo.com'},
            productImageUrls: [],
          },
        ];
        handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
        moduleElement =
            await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
        assertTrue(!!moduleElement);
        document.body.append(moduleElement);
        moduleElement.$.consentCardElement.render();

        consentCard = $$<DiscountConsentCard>(moduleElement, '#consentCardV2')!;
      });

      test(
          'Verify clicking accpet button in step 2 hides consent card',
          async () => testClickingAcceptButtonOnConsentCard());

      test(
          'Verify clicking reject button in step 2 hides consent card',
          async () => {
            // Arrange.
            assertEquals(true, isVisible(consentCard));
            const transitionend = eventToPromise(
                'transitionend', moduleElement.$.consentContainer);
            const consentToast = moduleElement.$.confirmDiscountConsentToast;
            assertEquals(
                0, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));
            nextStep(consentCard);
            await flushTasks();

            // Act.
            clickRejectButton(consentCard);
            await flushTasks();
            await transitionend;

            // Assert.
            assertEquals(false, isVisible(consentCard));
            assertEquals(true, consentToast.open);
            assertEquals(
                'Reject confirmation!',
                moduleElement.$.confirmDiscountConsentMessage.innerText);
            assertEquals(
                1, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));
          });
    });

    suite('Native Dialog consent variation', () => {
      let consentCard: DiscountConsentCard;

      setup(async () => {
        loadTimeData.overrideValues({
          modulesCartDiscountConsentVariation:
              DiscountConsentVariation.NATIVE_DIALOG,
        });

        const carts = [
          {
            merchant: 'Foo',
            cartUrl: {url: 'https://foo.com'},
            productImageUrls: [],
          },
          {
            merchant: 'Boo',
            cartUrl: {url: 'https://Boo.com'},
            productImageUrls: [],
          },
        ];
        handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
        moduleElement =
            await chromeCartDescriptor.initialize(0) as ChromeCartModuleElement;
        assertTrue(!!moduleElement);
        document.body.append(moduleElement);
        moduleElement.$.consentCardElement.render();

        consentCard = $$<DiscountConsentCard>(moduleElement, '#consentCardV2')!;
      });

      test(
          'Verify consent card is continue visible after consent dialog dimissed',
          async () => {
            // Arrange.
            assertEquals(true, isVisible(consentCard));
            handler.setResultFor(
                'showNativeConsentDialog',
                Promise.resolve({consentStatus: ConsentStatus.DISMISSED}));
            const transitionEndEvent = eventToPromise(
                'transitionend', moduleElement.$.consentContainer);
            const transitionTimeoutEvent = new Promise(resolve => {
              setTimeout(
                  resolve, 300, 'transition should be ended if there\'s any');
            });

            // Act.
            nextStep(consentCard);
            await flushTasks();
            await Promise.race([transitionEndEvent, transitionTimeoutEvent]);

            // Assert.
            assertEquals(true, isVisible(consentCard));
          });

      test(
          'Verify consent card is hidden and toast shows after acceptance',
          async () => {
            // Arrange.
            const consentToast = moduleElement.$.confirmDiscountConsentToast;
            assertEquals(true, isVisible(consentCard));
            handler.setResultFor(
                'showNativeConsentDialog',
                Promise.resolve({consentStatus: ConsentStatus.ACCEPTED}));
            const transitionEndEvent = eventToPromise(
                'transitionend', moduleElement.$.consentContainer);

            // Act.
            nextStep(consentCard);
            await flushTasks();
            await transitionEndEvent;

            // Assert.
            assertEquals(false, isVisible(consentCard));
            assertEquals(true, consentToast.open);
            assertEquals(
                'Accept confirmation!',
                moduleElement.$.confirmDiscountConsentMessage.innerText);
          });

      test(
          'Verify consent card is hidden and toast shows after rejection',
          async () => {
            // Arrange.
            const consentToast = moduleElement.$.confirmDiscountConsentToast;
            assertEquals(true, isVisible(consentCard));
            handler.setResultFor(
                'showNativeConsentDialog',
                Promise.resolve({consentStatus: ConsentStatus.REJECTED}));
            const transitionEndEvent = eventToPromise(
                'transitionend', moduleElement.$.consentContainer);

            // Act.
            nextStep(consentCard);
            await flushTasks();
            await transitionEndEvent;

            // Assert.
            assertEquals(false, isVisible(consentCard));
            assertEquals(true, consentToast.open);
            assertEquals(
                'Reject confirmation!',
                moduleElement.$.confirmDiscountConsentMessage.innerText);
          });
    });
  });
});
