// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, chromeCartDescriptor, ChromeCartProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertStyle} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesChromeCartModuleTest', () => {
  /**
   * @implements {ChromeCartProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = TestBrowserProxy.fromClass(ChromeCartProxy);
    testProxy.handler =
        TestBrowserProxy.fromClass(chromeCart.mojom.CartHandlerRemote);
    ChromeCartProxy.instance_ = testProxy;
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
    const cartItems =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.cart-item'));
    assertEquals(4, cartItems.length);

    assertEquals('https://amazon.com/', cartItems[0].href);
    assertEquals('Amazon', cartItems[0].querySelector('.merchant').innerText);
    let itemCount = cartItems[0].querySelector('.item-count').innerText;
    assertEquals('3', itemCount.slice(-1));
    let thumbnailList = Array.from(
        cartItems[0].querySelector('.thumbnail-list').querySelectorAll('img'));
    assertEquals(3, thumbnailList.length);
    assertEquals('https://image1.com', thumbnailList[0].autoSrc);
    assertEquals('https://image2.com', thumbnailList[1].autoSrc);
    assertEquals('https://image3.com', thumbnailList[2].autoSrc);
    assertEquals(null, cartItems[0].querySelector('.thumbnail-fallback'));

    assertEquals('https://ebay.com/', cartItems[1].href);
    assertEquals('eBay', cartItems[1].querySelector('.merchant').innerText);
    itemCount = cartItems[1].querySelector('.item-count').innerText;
    assertEquals('2', itemCount.slice(-1));
    thumbnailList = Array.from(
        cartItems[1].querySelector('.thumbnail-list').querySelectorAll('img'));
    assertEquals(2, thumbnailList.length);
    assertEquals('https://image4.com', thumbnailList[0].autoSrc);
    assertEquals('https://image5.com', thumbnailList[1].autoSrc);
    assertEquals(null, cartItems[1].querySelector('.thumbnail-fallback'));

    assertEquals('https://bestbuy.com/', cartItems[2].href);
    assertEquals('BestBuy', cartItems[2].querySelector('.merchant').innerText);
    assertEquals(null, cartItems[2].querySelector('.item-count'));
    assertEquals(null, cartItems[2].querySelector('.thumbnail-list'));
    assertEquals(
        'chrome://new-tab-page/icons/cart_fallback.svg',
        cartItems[2].querySelector('.thumbnail-fallback').src);

    assertEquals('https://walmart.com/', cartItems[3].href);
    assertEquals('Walmart', cartItems[3].querySelector('.merchant').innerText);
    itemCount = cartItems[3].querySelector('.item-count').innerText;
    assertEquals('4', itemCount.slice(-1));
    thumbnailList = Array.from(
        cartItems[3].querySelector('.thumbnail-list').querySelectorAll('img'));
    assertEquals(3, thumbnailList.length);
    assertEquals('https://image6.com', thumbnailList[0].autoSrc);
    assertEquals('https://image7.com', thumbnailList[1].autoSrc);
    assertEquals('https://image8.com', thumbnailList[2].autoSrc);
    assertEquals(null, cartItems[1].querySelector('.thumbnail-fallback'));
  });

  test('cart module header chip', async () => {
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

    // Act.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.cartItemRepeat.render();

    // Assert.
    const cartItems =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.cart-item'));
    assertEquals(1, cartItems.length);
    const headerChip =
        moduleElement.shadowRoot.querySelector('ntp-module-header')
            .shadowRoot.querySelector('#chip');
    assertEquals(
        loadTimeData.getString('modulesCartHeaderNew'), headerChip.innerText);
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

    // Arrange.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.cartItemRepeat.render();

    // Act.
    const waitForDismissEvent = eventToPromise('dismiss-module', moduleElement);
    const dismissButton =
        moduleElement.shadowRoot.querySelector('ntp-module-header')
            .shadowRoot.querySelector('#dismissButton');
    dismissButton.click();
    const dismissEvent = await waitForDismissEvent;
    const toastMessage = dismissEvent.detail.message;
    const restoreCallback = dismissEvent.detail.restoreCallback;

    // Assert.
    assertEquals('Your carts', toastMessage);
    assertEquals(1, testProxy.handler.getCallCount('dismissCartModule'));

    // Act.
    restoreCallback();

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('restoreCartModule'));
  });

  test('show and hide action menu', async () => {
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

    // Arrange.
    await chromeCartDescriptor.initialize();
    const moduleElement = chromeCartDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.cartItemRepeat.render();

    // Assert.
    const cartItems =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.cart-item'));
    assertEquals(1, cartItems.length);
    let menuButton = cartItems[0].querySelector('.icon-more-vert');
    assertStyle(menuButton, 'opacity', '0');
    const actionMenu = $$(moduleElement, '#actionMenu');
    assertFalse(actionMenu.open);

    // Act
    menuButton.click();

    // Assert.
    assertTrue(actionMenu.open);
  });
});
