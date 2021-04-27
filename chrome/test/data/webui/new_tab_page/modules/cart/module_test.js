// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, chromeCartDescriptor, ChromeCartProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {assertNotStyle} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
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

  test('creates no module if no cart item', async () => {
    // Arrange.
    testProxy.handler.setResultFor(
        'getMerchantCarts', Promise.resolve({carts: []}));

    // Act.
    await chromeCartDescriptor.initialize();

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('getMerchantCarts'));
    assertEquals(null, chromeCartDescriptor.element);
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
    testProxy.handler.setResultFor(
        'getMerchantCarts', Promise.resolve({carts}));

    // Act.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.cartItemRepeat.render();

    // Assert.
    const cartItems = moduleElement.shadowRoot.querySelectorAll('.cart-item');
    assertEquals(4, cartItems.length);
    assertEquals(220, chromeCartDescriptor.element.offsetHeight);

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
    assertEquals('BestBuy', cartItems[2].querySelector('.merchant').innerText);
    assertEquals(null, cartItems[2].querySelector('.item-count'));
    assertEquals(null, cartItems[2].querySelector('.thumbnail-list'));
    assertEquals(
        'chrome://new-tab-page/modules/cart/icons/cart_fallback.svg',
        cartItems[2].querySelector('.thumbnail-fallback').src);

    assertEquals('https://walmart.com/', cartItems[3].href);
    assertEquals('Walmart', cartItems[3].querySelector('.merchant').innerText);
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
    testProxy.handler.setResultFor(
        'getMerchantCarts', Promise.resolve({carts}));
    testProxy.handler.setResultFor(
        'getWarmWelcomeVisible', Promise.resolve({welcomeVisible: true}));

    // Arrange.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.cartItemRepeat.render();

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
    assertEquals(227, chromeCartDescriptor.element.offsetHeight);
  });

  test('Backend is notified when module is dismissed or restored', async () => {
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
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.cartItemRepeat.render();

    // Act.
    const waitForDismissEvent = eventToPromise('dismiss-module', moduleElement);
    $$(moduleElement, 'ntp-module-header')
        .dispatchEvent(new Event('dismiss-button-click', {bubbles: true}));
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
    assertEquals(1, testProxy.handler.getCallCount('restoreHiddenCartModule'));
    assertEquals(1, metrics.count('NewTabPage.Carts.UndoHideModule'));

    // Act.
    const waitForDisableEvent = eventToPromise('disable-module', moduleElement);
    $$(moduleElement, 'ntp-module-header')
        .dispatchEvent(new Event('disable-button-click', {bubbles: true}));
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
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
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

  test('scroll with full width module', async () => {
    // Arrange.
    const dummyMerchant = {
      merchant: 'Dummy',
      cartUrl: {url: 'https://dummy.com'},
      productImageUrls: [],
    };
    const carts = Array.from({length: 10}, () => dummyMerchant);
    testProxy.handler.setResultFor(
        'getMerchantCarts', Promise.resolve({carts}));

    // Arrange.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
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
    let waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
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
    testProxy.handler.setResultFor(
        'getMerchantCarts', Promise.resolve({carts}));

    // Arrange.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
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
    let waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
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
    testProxy.handler.setResultFor(
        'getMerchantCarts', Promise.resolve({carts}));

    // Arrange.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.cartItemRepeat.render();

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
    testProxy.handler.setResultFor(
        'getMerchantCarts', Promise.resolve({carts}));

    // Act.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
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
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.consentCardElement.render();

    // Assert.
    const consentCard = $$(moduleElement, '#consentCard');
    const consentToast =
        moduleElement.shadowRoot.querySelector('#confirmDiscountConsentToast');
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
        consentToast.querySelector('#confirmDiscountConsentMessage').innerText);

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
        consentToast.querySelector('#confirmDiscountConsentMessage').innerText);

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
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
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
    let waitForScrollFinished = eventToPromise('scroll-finish', moduleElement);
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

  function checkScrollButtonVisibility(
      moduleElement, isLeftVisible, isRightVisible) {
    assertEquals(
        isLeftVisible,
        isVisible(moduleElement.shadowRoot.querySelector('#leftScrollShadow')));
    assertEquals(
        isLeftVisible,
        isVisible(moduleElement.shadowRoot.querySelector('#leftScrollButton')));
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
    return (cart.offsetLeft > cartCarousel.scrollLeft) &&
        (cartCarousel.scrollLeft + cartCarousel.clientWidth) >
        (cart.offsetLeft + cart.offsetWidth);
  }
});
