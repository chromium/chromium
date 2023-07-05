// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://shopping-insights-side-panel.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ShoppingInsightsAppElement} from 'chrome://shopping-insights-side-panel.top-chrome/app.js';
import {ShoppingListApiProxyImpl} from 'chrome://shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {PageCallbackRouter, PriceInsightsInfo, PriceInsightsInfo_PriceBucket, ProductInfo} from 'chrome://shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ShoppingInsightsAppTest', () => {
  let shoppingInsightsApp: ShoppingInsightsAppElement;
  const shoppingListApi = TestMock.fromClass(ShoppingListApiProxyImpl);

  const productInfo: ProductInfo = {
    title: 'Product Foo',
    clusterTitle: 'Product Cluster Foo',
    domain: 'foo.com',
    imageUrl: {url: 'https://foo.com/image'},
    productUrl: {url: 'https://foo.com/product'},
    currentPrice: '$12',
    previousPrice: '$34',
    clusterId: BigInt(12345),
  };
  const priceInsights1: PriceInsightsInfo = {
    clusterId: BigInt(123),
    typicalLowPrice: '$100',
    typicalHighPrice: '$200',
    catalogAttributes: 'Unlocked, 4GB',
    jackpot: {url: 'https://foo.com/jackpot'},
    bucket: PriceInsightsInfo_PriceBucket.kLow,
    hasMultipleCatalogs: true,
    history: [{
      date: '2021-01-01',
      price: 100,
      formattedPrice: '$100',
    }],
    locale: 'en-us',
    currencyCode: 'usd',
  };
  const priceInsights2: PriceInsightsInfo = {
    clusterId: BigInt(123),
    typicalLowPrice: '$100',
    typicalHighPrice: '$100',
    catalogAttributes: 'Unlocked, 4GB',
    jackpot: {url: 'https://foo.com/jackpot'},
    bucket: PriceInsightsInfo_PriceBucket.kLow,
    hasMultipleCatalogs: false,
    history: [],
    locale: 'en-us',
    currencyCode: 'usd',
  };
  const priceInsights3: PriceInsightsInfo = {
    clusterId: BigInt(123),
    typicalLowPrice: '',
    typicalHighPrice: '',
    catalogAttributes: 'Unlocked, 4GB',
    jackpot: {url: 'https://foo.com/jackpot'},
    bucket: PriceInsightsInfo_PriceBucket.kHigh,
    hasMultipleCatalogs: false,
    history: [{
      date: '2021-01-01',
      price: 100,
      formattedPrice: '$100',
    }],
    locale: 'en-us',
    currencyCode: 'usd',
  };

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    shoppingListApi.reset();
    shoppingListApi.setResultFor(
        'getProductInfoForCurrentUrl',
        Promise.resolve({productInfo: productInfo}));
    shoppingListApi.setResultFor(
        'isShoppingListEligible', Promise.resolve({eligible: false}));
    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

    shoppingInsightsApp = document.createElement('shopping-insights-app');
  });

  test('HasBothRangeAndHistoryMultipleOptions', async () => {
    shoppingListApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights1}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await flushTasks();

    const panelTitle =
        shoppingInsightsApp.shadowRoot!.querySelector('.panel-title');
    assertTrue(!!panelTitle);
    assertEquals('Product Cluster Foo', panelTitle.textContent!.trim());

    const range = shoppingInsightsApp.shadowRoot!.querySelector('#priceRange');
    assertTrue(!!range);
    assertEquals(
        loadTimeData.getStringF('rangeMultipleOptions', '$100', '$200'),
        range.textContent!.trim());

    assertFalse(isVisible(
        shoppingInsightsApp.shadowRoot!.querySelector('#attributes1')));
    assertFalse(
        isVisible(shoppingInsightsApp.shadowRoot!.querySelector('#desc1')));
    assertTrue(isVisible(
        shoppingInsightsApp.shadowRoot!.querySelector('#historySection')));


    const historyTitle =
        shoppingInsightsApp.shadowRoot!.querySelector('#historyTitle');
    assertTrue(!!historyTitle);
    assertEquals(
        loadTimeData.getString('lowPriceMultipleOptions'),
        historyTitle.textContent!.trim());

    assertTrue(isVisible(shoppingInsightsApp.shadowRoot!.querySelector(
        'shopping-insights-history-graph')));
  });

  test('HasRangeOnlySingleOption', async () => {
    shoppingListApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights2}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await flushTasks();

    const panelTitle =
        shoppingInsightsApp.shadowRoot!.querySelector('.panel-title');
    assertTrue(!!panelTitle);
    assertEquals('Product Cluster Foo', panelTitle.textContent!.trim());

    const range = shoppingInsightsApp.shadowRoot!.querySelector('#priceRange');
    assertTrue(!!range);
    assertEquals(
        loadTimeData.getStringF('rangeSingleOptionOnePrice', '$100'),
        range.textContent!.trim());

    assertFalse(isVisible(
        shoppingInsightsApp.shadowRoot!.querySelector('#historySection')));
  });

  test('HasHistoryOnlySingleOption', async () => {
    shoppingListApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights3}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await flushTasks();

    const panelTitle =
        shoppingInsightsApp.shadowRoot!.querySelector('.panel-title');
    assertTrue(!!panelTitle);
    assertEquals('Product Cluster Foo', panelTitle.textContent!.trim());

    assertFalse(isVisible(
        shoppingInsightsApp.shadowRoot!.querySelector('#priceRange')));

    assertTrue(isVisible(
        shoppingInsightsApp.shadowRoot!.querySelector('#historySection')));

    const historyTitle =
        shoppingInsightsApp.shadowRoot!.querySelector('#historyTitle');
    assertTrue(!!historyTitle);
    assertEquals(
        loadTimeData.getString('highPriceSingleOption'),
        historyTitle.textContent!.trim());

    assertTrue(isVisible(shoppingInsightsApp.shadowRoot!.querySelector(
        'shopping-insights-history-graph')));
  });

  [true, false].forEach((eligible) => {
    test('PriceTrackingSectionVisibility', async () => {
      shoppingListApi.setResultFor(
          'isShoppingListEligible', Promise.resolve({eligible: eligible}));
      shoppingListApi.setResultFor(
          'getPriceInsightsInfoForCurrentUrl',
          Promise.resolve({priceInsightsInfo: priceInsights1}));
      shoppingListApi.setResultFor(
          'getPriceTrackingStatusForCurrentUrl',
          Promise.resolve({tracked: true}));
      const callbackRouter = new PageCallbackRouter();
      shoppingListApi.setResultFor('getCallbackRouter', callbackRouter);

      document.body.appendChild(shoppingInsightsApp);
      await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
      await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
      await shoppingListApi.whenCalled('isShoppingListEligible');
      await flushTasks();

      assertEquals(
          isVisible(shoppingInsightsApp.shadowRoot!.querySelector(
              '#priceTrackingSection')),
          eligible);
    });
  });
});
