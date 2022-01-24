// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, chromeCartDescriptor, ChromeCartProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {assertNotStyle, installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {eventToPromise, flushTasks, isVisible} from 'chrome://test/test_util.js';

suite('NewTabPageModulesChromeCartModuleTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  /** @type {!MetricsTracker} */
  let metrics;

  setup(() => {
    document.body.innerHTML = '';

    handler = installMock(
        chromeCart.mojom.CartHandlerRemote, ChromeCartProxy.setHandler);
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
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Act.
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(4, cartItems.length);
      assertEquals(220, moduleElement.offsetHeight);
      assertEquals(1, metrics.count('NewTabPage.Carts.CartCount', 4));

      assertEquals('https://amazon.com/', cartItems[0].href);
      assertEquals('Amazon', cartItems[0].querySelector('.merchant').innerText);
      let itemCount = cartItems[0].querySelector('.item-count').innerText;
      assertEquals('3', itemCount.slice(-1));
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
      assertEquals('2', itemCount.slice(-1));
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
      assertEquals('4', itemCount.slice(-1));
      thumbnailList =
          cartItems[3].querySelector('.thumbnail-list').querySelectorAll('img');
      assertEquals(3, thumbnailList.length);
      assertEquals('https://image6.com', thumbnailList[0].autoSrc);
      assertEquals('https://image7.com', thumbnailList[1].autoSrc);
      assertEquals('https://image8.com', thumbnailList[2].autoSrc);
      assertEquals(null, cartItems[1].querySelector('.thumbnail-fallback'));
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
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

      // Assert.
      const headerChip =
          moduleElement.shadowRoot.querySelector('ntp-module-header')
              .shadowRoot.querySelector('#chip');
      const headerDescription =
          moduleElement.shadowRoot.querySelector('ntp-module-header')
              .shadowRoot.querySelector('#description');
      assertEquals(
          loadTimeData.getString('modulesCartHeaderNew'), headerChip.innerText);
      assertEquals(
          loadTimeData.getString('modulesCartWarmWelcome'),
          headerDescription.innerText);
      assertEquals(227, moduleElement.offsetHeight);
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
          handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
          loadTimeData.overrideValues({
            disableModuleToastMessage: 'hello $1',
            modulesCartLowerYour: 'world',
          });

          // Arrange.
          const moduleElement =
              assert(await chromeCartDescriptor.initialize(0));
          document.body.append(moduleElement);
          $$(moduleElement, '#cartItemRepeat').render();

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
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Arrange.
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

      // Assert.
      const cartContainers =
          moduleElement.shadowRoot.querySelectorAll('.cart-container');
      assertEquals(2, cartContainers.length);
      let menuButton = cartContainers[0].querySelector('.icon-more-vert');
      const actionMenu = $$(moduleElement, '#cartActionMenu');
      assertFalse(actionMenu.open);

      // Act
      menuButton.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, handler.getCallCount('hideCart'));

      // Act
      actionMenu.querySelector('#hideCartButton').click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('hideCart'));
      assertTrue($$(moduleElement, '#dismissCartToast').open);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuHideMerchantToastMessage', 'Foo'),
          assert($$(moduleElement, '#dismissCartToastMessage'))
              .textContent.trim());
      assertNotStyle(
          assert($$(moduleElement, '#undoDismissCartButton')), 'display',
          'none');
      assertEquals(0, handler.getCallCount('restoreHiddenCart'));

      // Act.
      $$(moduleElement, '#undoDismissCartButton').click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('restoreHiddenCart'));
      assertFalse(actionMenu.open);

      // Act.
      menuButton = cartContainers[1].querySelector('.icon-more-vert');
      menuButton.click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, handler.getCallCount('removeCart'));

      // Act
      actionMenu.querySelector('#removeCartButton').click();
      await flushTasks();

      // Assert.
      assertEquals(1, handler.getCallCount('removeCart'));
      assertTrue($$(moduleElement, '#dismissCartToast').open);
      assertEquals(
          loadTimeData.getStringF(
              'modulesCartCartMenuRemoveMerchantToastMessage', 'Bar'),
          $$(moduleElement, '#dismissCartToastMessage').textContent.trim());
      assertNotStyle(
          assert($$(moduleElement, '#undoDismissCartButton')), 'display',
          'none');
      assertEquals(0, handler.getCallCount('restoreRemovedCart'));

      // Act
      $$(moduleElement, '#undoDismissCartButton').click();
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
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

      // Assert.
      let cartContainers =
          moduleElement.shadowRoot.querySelectorAll('.cart-container');
      assertEquals(1, cartContainers.length);
      const actionMenu = $$(moduleElement, '#cartActionMenu');
      assertFalse(actionMenu.open);

      // Act.
      cartContainers[0].querySelector('.icon-more-vert').click();

      // Assert.
      assertTrue(actionMenu.open);
      assertEquals(0, handler.getCallCount('hideCart'));

      // Act.
      carts = [];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      const waitForDismissEvent =
          eventToPromise('dismiss-module', moduleElement);
      actionMenu.querySelector('#hideCartButton').click();
      const hideEvent = await waitForDismissEvent;
      const hideToastMessage = hideEvent.detail.message;
      const hideRestoreCallback = hideEvent.detail.restoreCallback;
      cartContainers = moduleElement.shadowRoot.querySelectorAll('.cart-item');

      // Assert.
      assertFalse($$(moduleElement, '#dismissCartToast').open);
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
      cartContainers = moduleElement.shadowRoot.querySelectorAll('.cart-item');

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
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
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
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
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
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
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
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();
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
      let waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollVisibilityChange =
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
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
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
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
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(4, cartItems.length);

      // Act.
      let waitForLeftScrollVisibilityChange =
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
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

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
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      handler.setResultFor(
          'getDiscountConsentCardVisible',
          Promise.resolve({consentVisible: true}));
      loadTimeData.overrideValues({
        modulesCartDiscountConsentRejectConfirmation: 'Reject confirmation!',
        modulesCartDiscountConsentAcceptConfirmation: 'Accept confirmation!',
      });

      // Arrange.
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#consentCardElement').render();

      // Assert.
      const consentCard = assert($$(moduleElement, '#consentCard'));
      const consentToast = assert(moduleElement.shadowRoot.querySelector(
          '#confirmDiscountConsentToast'));
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
      assertEquals(0, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));

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
      assertEquals(1, metrics.count('NewTabPage.Carts.RejectDiscountConsent'));

      // Act.
      consentToast.querySelector('#confirmDiscountConsentButton').click();

      // Assert.
      assertEquals(false, consentToast.open);
      assertEquals(false, isVisible(consentCard));

      // Act.
      moduleElement.showDiscountConsent = true;
      $$(moduleElement, '#consentCardElement').render();

      // Assert.
      assertEquals(true, isVisible(consentCard));
      assertEquals(0, metrics.count('NewTabPage.Carts.AcceptDiscountConsent'));

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
      assertEquals(1, metrics.count('NewTabPage.Carts.AcceptDiscountConsent'));
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
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();
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
      let waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-hide', moduleElement);
      let waitForRightScrollVisibilityChange =
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
      let waitForScrollFinished =
          eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot.querySelector('#rightScrollButton').click();
      await waitForScrollFinished;
      await waitForLeftScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, true, true);
      checkVisibleRange(moduleElement, 2, 5);

      // Act.
      waitForLeftScrollVisibilityChange =
          eventToPromise('left-scroll-hide', moduleElement);
      waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
      moduleElement.shadowRoot.querySelector('#leftScrollButton').click();
      await waitForScrollFinished;
      await waitForLeftScrollVisibilityChange;

      // Assert.
      checkScrollButtonVisibility(moduleElement, false, true);
      checkVisibleRange(moduleElement, 0, 1);
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
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      // Act.
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

      // Assert.
      const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
      assertEquals(4, cartItems.length);

      // Act.
      cartItems[3].querySelector('.cart-title').click();
      cartItems[0].click();
      cartItems[2].querySelector('.favicon-image').click();
      cartItems[1].querySelector('.thumbnail-container').click();

      // Assert.
      assertEquals(4, handler.getCallCount('prepareForNavigation'));
      for (let index = 0; index < 4; index++) {
        assertEquals(1, metrics.count('NewTabPage.Carts.ClickCart', index));
        assertEquals(true, handler.getArgs('prepareForNavigation')[index][1]);
      }
      assertEquals(0, handler.getCallCount('getDiscountURL'));
    });

    function checkScrollButtonVisibility(
        moduleElement, isLeftVisible, isRightVisible) {
      assertEquals(
          isLeftVisible,
          isVisible(
              moduleElement.shadowRoot.querySelector('#leftScrollShadow')));
      assertEquals(
          isLeftVisible,
          isVisible(
              moduleElement.shadowRoot.querySelector('#leftScrollButton')));
      assertEquals(
          isRightVisible,
          isVisible(
              moduleElement.shadowRoot.querySelector('#rightScrollShadow')));
      assertEquals(
          isRightVisible,
          isVisible(
              moduleElement.shadowRoot.querySelector('#rightScrollButton')));
    }

    function checkVisibleRange(moduleElement, startIndex, endIndex) {
      const carts = moduleElement.shadowRoot.querySelector('#cartCarousel')
                        .querySelectorAll('.cart-container');
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
      const cart = cartCarousel.querySelectorAll('.cart-container')[index];
      return (cart.offsetLeft > cartCarousel.scrollLeft) &&
          (cartCarousel.scrollLeft + cartCarousel.clientWidth) >
          (cart.offsetLeft + cart.offsetWidth);
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
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));
      handler.setResultFor(
          'getDiscountURL',
          Promise.resolve({discountUrl: {url: 'https://www.foo.com'}}));

      // Act.
      const moduleElement = assert(await chromeCartDescriptor.initialize(0));
      document.body.append(moduleElement);
      $$(moduleElement, '#cartItemRepeat').render();

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
          cartItems[2].querySelector('.favicon-image'), 'https://bestbuy.com',
          2, 3, moduleElement);
      await testRBDCartClick(
          cartItems[1].querySelector('.thumbnail-container'),
          'https://ebay.com', 1, 4, moduleElement);

      // Act.
      const cartContainers =
          moduleElement.shadowRoot.querySelectorAll('.cart-container');
      cartContainers[0].querySelector('.icon-more-vert').click();

      // Assert.
      assertEquals(4, handler.getCallCount('getDiscountURL'));
    });

    async function testRBDCartClick(
        clickingElement, cartURL, index, expectedCallCount, moduleElement) {
      // Act.
      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      clickingElement.click();

      // Assert.
      assertEquals(0, metrics.count('NewTabPage.Carts.ClickCart', index));
      assertEquals(expectedCallCount, handler.getCallCount('getDiscountURL'));
      assertEquals(
          cartURL,
          handler.getArgs('getDiscountURL')[expectedCallCount - 1].url);

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
      const moduleElement = await chromeCartDescriptor.initialize(0);

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
          discountText: '5% off'
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
          discountText: '10% off'
        },
      ];
      handler.setResultFor('getMerchantCarts', Promise.resolve({carts}));

      assertEquals(0, metrics.count('NewTabPage.Carts.DiscountCountAtLoad', 2));
      assertEquals(0, metrics.count('NewTabPage.Carts.DiscountAt', 0));
      assertEquals(0, metrics.count('NewTabPage.Carts.DiscountAt', 2));

      // Act.
      const moduleElement = await chromeCartDescriptor.initialize(0);

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Carts.DiscountCountAtLoad', 2));
      assertEquals(1, metrics.count('NewTabPage.Carts.DiscountAt', 0));
      assertEquals(1, metrics.count('NewTabPage.Carts.DiscountAt', 2));
    });
  });
});
