// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://shopping-insights-side-panel.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ShoppingInsightsAppElement} from 'chrome://shopping-insights-side-panel.top-chrome/app.js';
import {ShoppingListApiProxyImpl} from 'chrome://shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {PriceInsightsInfo, PriceInsightsInfo_PriceBucket, ProductInfo} from 'chrome://shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
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
  };
  const priceInsights1: PriceInsightsInfo = {
    clusterId: BigInt(123),
    typicalLowPrice: '$100',
    typicalHighPrice: '$200',
    catalogAttributes: 'Unlocked, 4GB',
    jackpot: {url: 'https://foo.com/jackpot'},
    bucket: PriceInsightsInfo_PriceBucket.kLow,
    hasMultipleCatalogs: true,
  };
  const priceInsights2: PriceInsightsInfo = {
    clusterId: BigInt(123),
    typicalLowPrice: '$100',
    typicalHighPrice: '$100',
    catalogAttributes: 'Unlocked, 4GB',
    jackpot: {url: 'https://foo.com/jackpot'},
    bucket: PriceInsightsInfo_PriceBucket.kLow,
    hasMultipleCatalogs: false,
  };
  const priceInsights3: PriceInsightsInfo = {
    clusterId: BigInt(123),
    typicalLowPrice: '',
    typicalHighPrice: '',
    catalogAttributes: 'Unlocked, 4GB',
    jackpot: {url: 'https://foo.com/jackpot'},
    bucket: PriceInsightsInfo_PriceBucket.kLow,
    hasMultipleCatalogs: true,
  };

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    shoppingListApi.reset();
    shoppingListApi.setResultFor(
        'getProductInfoForCurrentUrl',
        Promise.resolve({productInfo: productInfo}));
    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

    shoppingInsightsApp = document.createElement('shopping-insights-app');
  });

  test('TitleSectionWithRangeMultipleOptions', async () => {
    shoppingListApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights1}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await flushTasks();

    assertEquals(
        'Product Cluster Foo',
        shoppingInsightsApp.shadowRoot!.querySelector(
                                           '.panel-title')!.textContent);
    assertEquals(
        loadTimeData.getStringF('rangeMultipleOptions', '$100', '$200'),
        shoppingInsightsApp.shadowRoot!.querySelector(
                                           '.panel-subtitle')!.textContent);
  });

  test('TitleSectionWithRangeSingleOptionOnePrice', async () => {
    shoppingListApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights2}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await flushTasks();

    assertEquals(
        'Product Cluster Foo',
        shoppingInsightsApp.shadowRoot!.querySelector(
                                           '.panel-title')!.textContent);
    assertEquals(
        loadTimeData.getStringF('rangeSingleOptionOnePrice', '$100'),
        shoppingInsightsApp.shadowRoot!.querySelector(
                                           '.panel-subtitle')!.textContent);
  });

  test('TitleSectionWithoutRange', async () => {
    shoppingListApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights3}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await flushTasks();

    assertEquals(
        'Product Cluster Foo',
        shoppingInsightsApp.shadowRoot!.querySelector(
                                           '.panel-title')!.textContent);
    assertFalse(isVisible(
        shoppingInsightsApp.shadowRoot!.querySelector('.panel-subtitle')));
  });
});
