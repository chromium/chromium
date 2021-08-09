// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, ChromeCartProxy, chromeCartV2Descriptor} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {assertNotStyle} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {eventToPromise, flushTasks, isVisible} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesChromeCartModuleTest', () => {
  /**
   * @implements {ChromeCartProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  /** @type {MetricsTracker} */
  let metrics;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = TestBrowserProxy.fromClass(ChromeCartProxy);
    testProxy.handler =
        TestBrowserProxy.fromClass(chromeCart.mojom.CartHandlerRemote);
    ChromeCartProxy.setInstance(testProxy);
    metrics = fakeMetricsPrivate();
    // Not show welcome surface by default.
    testProxy.handler.setResultFor(
        'getWarmWelcomeVisible', Promise.resolve({welcomeVisible: false}));
    // Not show consent card by default.
    testProxy.handler.setResultFor(
        'getDiscountConsentCardVisible',
        Promise.resolve({consentVisible: false}));
  });

  suite('Normal cart without rule-based discount', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({ruleBasedDiscountEnabled: false});
    });

    test('creates no module if no cart item', async () => {
      // Arrange.
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts: []}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize();

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('getMerchantCarts'));
      assertEquals(null, moduleElement);
    });

    test('creates module if cart item', async () => {
      const carts = [
        {
          merchant: 'Amazon',
          cartUrl: {url: 'https://amazon.com'},
          productImageUrls: [
            {url: 'https://image1.com'}, {url: 'https://image2.com'},
            {url: 'https://image3.com'}
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
            {url: 'https://image6.com'}, {url: 'https://image7.com'},
            {url: 'https://image8.com'}, {url: 'https://image9.com'}
          ],
        },
        {
          merchant: 'Nike',
          cartUrl: {url: 'https://nike.com'},
          productImageUrls: [{url: 'https://image10.com'}],
        },
      ];
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));
      loadTimeData.overrideValues({
        modulesCartItemCountSingular: '$1 item',
        modulesCartItemCountMultiple: '$1 items',
      });

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(5, cartItems.length);
      assertEquals(446, moduleElement.offsetHeight);
      assertEquals(1, metrics.count('NewTabPage.Carts.CartCount', 5));

      assertEquals('https://amazon.com/', cartItems[0].href);
      assertEquals('Amazon', cartItems[0].querySelector('.merchant').innerText);
      let itemCount = cartItems[0].querySelector('.item-count').innerText;
      assertEquals('3 items', itemCount);
      let thumbnailList =
          cartItems[0].querySelector('.thumbnail-list').querySelectorAll('img');
      assertEquals(3, thumbnailList.length);
      assertEquals('https://image1.com', thumbnailList[0].autoSrc);
      assertEquals('https://image2.com', thumbnailList[1].autoSrc);
      assertEquals('https://image3.com', thumbnailList[2].autoSrc);
      assertEquals(null, cartItems[0].querySelector('.thumbnail-fallback'));

      assertEquals('https://ebay.com/', cartItems[1].href);
      assertEquals('eBay', cartItems[1].querySelector('.merchant').innerText);
      itemCount = cartItems[1].querySelector('.item-count').innerText;
      assertEquals('2 items', itemCount);
      thumbnailList =
          cartItems[1].querySelector('.thumbnail-list').querySelectorAll('img');
      assertEquals(2, thumbnailList.length);
      assertEquals('https://image4.com', thumbnailList[0].autoSrc);
      assertEquals('https://image5.com', thumbnailList[1].autoSrc);
      assertEquals(null, cartItems[1].querySelector('.thumbnail-fallback'));

      assertEquals('https://bestbuy.com/', cartItems[2].href);
      assertEquals(
          'BestBuy', cartItems[2].querySelector('.merchant').innerText);
      assertEquals(null, cartItems[2].querySelector('.item-count'));
      assertEquals(null, cartItems[2].querySelector('.thumbnail-list'));
      assertEquals(
          'chrome://new-tab-page/modules/cart/icons/cart_fallback.svg',
          cartItems[2].querySelector('.thumbnail-fallback').src);

      assertEquals('https://walmart.com/', cartItems[3].href);
      assertEquals(
          'Walmart', cartItems[3].querySelector('.merchant').innerText);
      itemCount = cartItems[3].querySelector('.item-count').innerText;
      assertEquals('4 items', itemCount);
      thumbnailList =
          cartItems[3].querySelector('.thumbnail-list').querySelectorAll('img');
      assertEquals(3, thumbnailList.length);
      assertEquals('https://image6.com', thumbnailList[0].autoSrc);
      assertEquals('https://image7.com', thumbnailList[1].autoSrc);
      assertEquals('https://image8.com', thumbnailList[2].autoSrc);
      assertEquals(null, cartItems[1].querySelector('.thumbnail-fallback'));

      assertEquals('https://nike.com/', cartItems[4].href);
      assertEquals('Nike', cartItems[4].querySelector('.merchant').innerText);
      itemCount = cartItems[4].querySelector('.item-count').innerText;
      assertEquals('1 item', itemCount);
      thumbnailList =
          cartItems[4].querySelector('.thumbnail-list').querySelectorAll('img');
      assertEquals(1, thumbnailList.length);
      assertEquals('https://image10.com', thumbnailList[0].autoSrc);
      assertEquals(null, cartItems[4].querySelector('.thumbnail-fallback'));
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
                {url: 'https://image1.com'}, {url: 'https://image2.com'},
                {url: 'https://image3.com'}
              ],
            },
          ];
          testProxy.handler.setResultFor(
              'getMerchantCarts', Promise.resolve({carts}));
          loadTimeData.overrideValues({
            disableModuleToastMessage: 'hello $1',
            modulesCartLowerYour: 'world',
          });

          // Arrange.
          const moduleElement = await chromeCartV2Descriptor.initialize();
          document.body.append(moduleElement);
          moduleElement.$.cartItemRepeat.render();

          // Act.
          const waitForDismissEvent =
              eventToPromise('dismiss-module', moduleElement);
          $$(moduleElement, 'ntp-module-header')
              .dispatchEvent(
                  new Event('dismiss-button-click', {bubbles: true}));
          const hideEvent = await waitForDismissEvent;
          const hideToastMessage = hideEvent.detail.message;
          const hideRestoreCallback = hideEvent.detail.restoreCallback;

          // Assert.
          assertEquals(
              loadTimeData.getString('modulesCartModuleMenuHideToastMessage'),
              hideToastMessage);
          assertEquals(1, testProxy.handler.getCallCount('hideCartModule'));
          assertEquals(1, metrics.count('NewTabPage.Carts.HideModule'));

          // Act.
          hideRestoreCallback();

          // Assert.
          assertEquals(
              1, testProxy.handler.getCallCount('restoreHiddenCartModule'));
          assertEquals(1, metrics.count('NewTabPage.Carts.UndoHideModule'));

          // Act.
          const waitForDisableEvent =
              eventToPromise('disable-module', moduleElement);
          $$(moduleElement, 'ntp-module-header')
              .dispatchEvent(
                  new Event('disable-button-click', {bubbles: true}));
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(2, cartItems.length);
      let menuButton = cartItems[0].querySelector('.icon-more-vert');
      const actionMenu = $$(moduleElement, '#cartActionMenu');
      assertFalse(actionMenu.open);

      // Act
      menuButton.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, testProxy.handler.getCallCount('hideCart'));

      // Act
      actionMenu.querySelector('#hideCartButton').click();
      await flushTasks();

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('hideCart'));
      assertTrue($$(moduleElement, '#dismissCartToast').open);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuHideMerchantToastMessage', 'Foo'),
          $$(moduleElement, '#dismissCartToastMessage').textContent.trim());
      assertNotStyle(
          $$(moduleElement, '#undoDismissCartButton'), 'display', 'none');
      assertEquals(0, testProxy.handler.getCallCount('restoreHiddenCart'));

      // Act.
      $$(moduleElement, '#undoDismissCartButton').click();
      await flushTasks();

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('restoreHiddenCart'));
      assertFalse(actionMenu.open);

      // Act.
      menuButton = cartItems[1].querySelector('.icon-more-vert');
      menuButton.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, testProxy.handler.getCallCount('removeCart'));

      // Act
      actionMenu.querySelector('#removeCartButton').click();
      await flushTasks();

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('removeCart'));
      assertTrue($$(moduleElement, '#dismissCartToast').open);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuRemoveMerchantToastMessage', 'Bar'),
          $$(moduleElement, '#dismissCartToastMessage').textContent.trim());
      assertNotStyle(
          $$(moduleElement, '#undoDismissCartButton'), 'display', 'none');
      assertEquals(0, testProxy.handler.getCallCount('restoreRemovedCart'));

      // Act
      $$(moduleElement, '#undoDismissCartButton').click();
      await flushTasks();

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('restoreRemovedCart'));
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      let cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(1, cartItems.length);
      const actionMenu = $$(moduleElement, '#cartActionMenu');
      assertFalse(actionMenu.open);

      // Act.
      cartItems[0].querySelector('.icon-more-vert').click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, testProxy.handler.getCallCount('hideCart'));

      // Act.
      carts = [];
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));
      const waitForDismissEvent =
          eventToPromise('dismiss-module', moduleElement);
      actionMenu.querySelector('#hideCartButton').click();
      const hideEvent = await waitForDismissEvent;
      const hideToastMessage = hideEvent.detail.message;
      const hideRestoreCallback = hideEvent.detail.restoreCallback;
      cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');

      // Assert.
      assertFalse($$(moduleElement, '#dismissCartToast').open);
      assertEquals(1, testProxy.handler.getCallCount('hideCart'));
      assertEquals(0, cartItems.length);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuHideMerchantToastMessage', 'Foo'),
          hideToastMessage);
      assertEquals(
          1, metrics.count('NewTabPage.Carts.DismissLastCartHidesModule'));

      // Act.
      carts = data;
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));
      hideRestoreCallback();
      await flushTasks();
      cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('restoreHiddenCart'));
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel =
          moduleElement.shadowRoot.querySelector('#cartCarousel');
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
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
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
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
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel =
          moduleElement.shadowRoot.querySelector('#cartCarousel');
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
      await waitForLeftScrollEnableChange;
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, true);
      checkVisibleRange(moduleElement, 3, 4);

      // Act.
      waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel =
          moduleElement.shadowRoot.querySelector('#cartCarousel');
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
      await waitForLeftScrollEnableChange;
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, true);
      checkVisibleRange(moduleElement, 3, 3);

      // Act.
      waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(3, cartItems.length);

      assertEquals('https://amazon.com/', cartItems[0].href);
      assertEquals(
          '15% off', cartItems[0].querySelector('.discount-chip').innerText);
      assertEquals('https://ebay.com/', cartItems[1].href);
      assertEquals(
          '50$ off', cartItems[1].querySelector('.discount-chip').innerText);
      assertEquals('https://bestbuy.com/', cartItems[2].href);
      assertEquals(null, cartItems[2].querySelector('.discount-chip'));
    });

    test('shows and hides discount consent in cart module', async () => {
      const carts = [
        {
          merchant: 'Foo',
          cartUrl: {url: 'https://foo.com'},
          productImageUrls: [],
        },
      ];
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));
      testProxy.handler.setResultFor(
          'getDiscountConsentCardVisible',
          Promise.resolve({consentVisible: true}));
      loadTimeData.overrideValues({
        modulesCartDiscountConsentRejectConfirmation: 'Reject confirmation!',
        modulesCartDiscountConsentAcceptConfirmation: 'Accept confirmation!',
      });

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.consentCardElement.render();

      // Assert.
      const consentCard = $$(moduleElement, '#consentCard');
      const consentToast = moduleElement.shadowRoot.querySelector(
          '#confirmDiscountConsentToast');
      assertEquals(true, isVisible(consentCard));
      assertEquals(false, consentToast.open);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentContent'),
          consentCard.querySelector('#consentContent').innerText);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentReject'),
          consentCard.querySelector('#cancelButton').innerText);
      assertEquals(
          loadTimeData.getString('modulesCartDiscountConsentAccept'),
          consentCard.querySelector('#actionButton').innerText);

      // Act.
      consentCard.querySelector('#cancelButton').click();
      await flushTasks();

      // Assert.
      assertEquals(false, isVisible(consentCard));
      assertEquals(true, consentToast.open);
      assertEquals(
          'Reject confirmation!',
          consentToast.querySelector('#confirmDiscountConsentMessage')
              .innerText);

      // Act.
      consentToast.querySelector('#confirmDiscountConsentButton').click();

      // Assert.
      assertEquals(false, consentToast.open);
      assertEquals(false, isVisible(consentCard));

      // Act.
      moduleElement.showDiscountConsent = true;
      moduleElement.$.consentCardElement.render();

      // Assert.
      assertEquals(true, isVisible(consentCard));

      // Act.
      consentCard.querySelector('#actionButton').click();
      await flushTasks();

      // Assert.
      assertEquals(false, isVisible(consentCard));
      assertEquals(true, consentToast.open);
      assertEquals(
          'Accept confirmation!',
          consentToast.querySelector('#confirmDiscountConsentMessage')
              .innerText);

      // Act.
      consentToast.querySelector('#confirmDiscountConsentButton').click();

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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));
      testProxy.handler.setResultFor(
          'getDiscountConsentCardVisible',
          Promise.resolve({consentVisible: true}));

      // Arrange.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();
      const cartCarousel =
          moduleElement.shadowRoot.querySelector('#cartCarousel');
      moduleElement.scrollBehavior = 'auto';
      const onScroll = () => {
        moduleElement.dispatchEvent(new Event('scroll-finish'));
      };
      cartCarousel.addEventListener('scroll', onScroll, false);

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(10, cartItems.length);

      // Act.
      let waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollEnableChange =
          eventToPromise('right-scroll-show', moduleElement);
      await waitForLeftScrollEnableChange;
      await waitForRightScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 1);

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-show', moduleElement);
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
      await waitForLeftScrollEnableChange;
      await waitForScrollFinished;

      // Assert.
      checkScrollButtonDisabled(moduleElement, false, false);
      checkVisibleRange(moduleElement, 2, 4);

      // Act.
      waitForLeftScrollEnableChange =
          eventToPromise('left-scroll-hide', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
      await waitForScrollFinished;
      await waitForLeftScrollEnableChange;

      // Assert.
      checkScrollButtonDisabled(moduleElement, true, false);
      checkVisibleRange(moduleElement, 0, 1);

      // Remove the observer.
      cartCarousel.removeEventListener('scroll', onScroll);
    });

    test('click on cart item', async () => {
      const carts = [
        {
          merchant: 'Amazon',
          cartUrl: {url: 'https://amazon.com'},
          productImageUrls: [
            {url: 'https://image1.com'}, {url: 'https://image2.com'},
            {url: 'https://image3.com'}
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(4, cartItems.length);

      // Act.
      cartItems[3].querySelector('.cart-title').click();
      cartItems[0].click();
      cartItems[2].querySelector('.icon-more-vert').click();
      cartItems[1].querySelector('.thumbnail-container').click();

      // Assert.
      Array(4).forEach(
          index => assertEquals(
              1, metrics.count('NewTabPage.Carts.ClickCart', index)));
      assertEquals(0, testProxy.handler.getCallCount('getDiscountURL'));
    });

    function checkScrollButtonDisabled(
        moduleElement, isLeftDisabled, isRightDisabled) {
      assertEquals(
          isLeftDisabled,
          moduleElement.shadowRoot.querySelector('#leftScrollButton').disabled);
      assertEquals(
          isRightDisabled,
          moduleElement.shadowRoot.querySelector('#rightScrollButton')
              .disabled);
    }

    function checkVisibleRange(moduleElement, startIndex, endIndex) {
      const carts = moduleElement.shadowRoot.querySelector('#cartCarousel')
                        .querySelectorAll('.cart-item');
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

    function getVisibilityForIndex(moduleElement, index) {
      const cartCarousel =
          moduleElement.shadowRoot.querySelector('#cartCarousel');
      const cart = cartCarousel.querySelectorAll('.cart-item')[index];
      return cart && (cart.offsetLeft === cartCarousel.scrollLeft) &&
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
            {url: 'https://image1.com'}, {url: 'https://image2.com'},
            {url: 'https://image3.com'}
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
      testProxy.handler.setResultFor(
          'getMerchantCarts', Promise.resolve({carts}));
      testProxy.handler.setResultFor(
          'getDiscountURL',
          Promise.resolve({discountUrl: {url: 'https://www.foo.com'}}));

      // Act.
      const moduleElement = await chromeCartV2Descriptor.initialize();
      document.body.append(moduleElement);
      moduleElement.$.cartItemRepeat.render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(4, cartItems.length);

      // Act and assert.
      await testRBDCartClick(
          cartItems[3].querySelector('.discount-chip'), 'https://walmart.com',
          3, 1, moduleElement);
      await testRBDCartClick(
          cartItems[0], 'https://amazon.com', 0, 2, moduleElement);
      await testRBDCartClick(
          cartItems[2], 'https://bestbuy.com', 2, 3, moduleElement);
      await testRBDCartClick(
          cartItems[1].querySelector('.thumbnail-container'),
          'https://ebay.com', 1, 4, moduleElement);

      // Act.
      cartItems[0].querySelector('.icon-more-vert').click();

      // Assert.
      assertEquals(4, testProxy.handler.getCallCount('getDiscountURL'));
    });

    async function testRBDCartClick(
        clickingElement, cartURL, index, expectedCallCount, moduleElement) {
      // Act.
      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      clickingElement.click();

      // Assert.
      assertEquals(0, metrics.count('NewTabPage.Carts.ClickCart', index));
      assertEquals(
          expectedCallCount, testProxy.handler.getCallCount('getDiscountURL'));
      assertEquals(
          cartURL,
          testProxy.handler.getArgs('getDiscountURL')[expectedCallCount - 1]
              .url);

      // Act.
      // Wait for the following click event triggered by the first click to
      // propagate.
      await waitForUsageEvent;

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Carts.ClickCart', index));
      assertEquals(
          'https://www.foo.com/',
          moduleElement.shadowRoot.querySelectorAll('.cart-item')[index].href);
    }
  });
});
