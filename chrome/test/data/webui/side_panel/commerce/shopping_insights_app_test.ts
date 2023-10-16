// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shopping-insights-side-panel.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {ShoppingInsightsAppElement} from 'chrome://shopping-insights-side-panel.top-chrome/app.js';
import {PriceTrackingSection} from 'chrome://shopping-insights-side-panel.top-chrome/price_tracking_section.js';
import {ShoppingListApiProxyImpl} from 'chrome://shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {PageCallbackRouter, PriceInsightsInfo, PriceInsightsInfo_PriceBucket, ProductInfo} from 'chrome://shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ShoppingInsightsAppTest', () => {
  let shoppingInsightsApp: ShoppingInsightsAppElement;
  const shoppingListApi = TestMock.fromClass(ShoppingListApiProxyImpl);
  let metrics: MetricsTracker;

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
  const priceInsights4: PriceInsightsInfo = {
    clusterId: BigInt(123),
    typicalLowPrice: '',
    typicalHighPrice: '',
    catalogAttributes: 'Unlocked, 4GB',
    jackpot: {url: ''},
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

    metrics = fakeMetricsPrivate();
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

    const titleSection =
        shoppingInsightsApp.shadowRoot!.querySelector('#titleSection');
    assertTrue(!!titleSection);
    assertFalse(
        isVisible(titleSection.querySelector('catalog-attributes-row')));
    assertFalse(isVisible(titleSection.querySelector('insights-comment-row')));

    const historySection =
        shoppingInsightsApp.shadowRoot!.querySelector('#historySection');
    assertTrue(!!historySection);
    assertTrue(isVisible(historySection));

    const historyTitle = historySection.querySelector('#historyTitle');
    assertTrue(!!historyTitle);
    assertTrue(isVisible(historyTitle));
    assertEquals(
        loadTimeData.getString('historyTitleMultipleOptions'),
        historyTitle.textContent!.trim());

    const attributesRow =
        historySection.querySelector('catalog-attributes-row');
    assertTrue(!!attributesRow);
    assertTrue(isVisible(attributesRow));

    const attributes = attributesRow.shadowRoot!.querySelector('.attributes');
    assertTrue(!!attributes);
    assertEquals('Unlocked, 4GB', attributes.textContent!.trim());

    const buyOption = attributesRow.shadowRoot!.querySelector('.link');
    assertTrue(!!buyOption);
    assertEquals(
        loadTimeData.getString('buyOptions'), buyOption.textContent!.trim());

    const button = attributesRow.shadowRoot!.querySelector('iron-icon');
    assertTrue(!!button);
    button.click();
    const url = await shoppingListApi.whenCalled('openUrlInNewTab');
    assertEquals('https://foo.com/jackpot', url.url);
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceInsights.BuyingOptionsClicked',
            PriceInsightsInfo_PriceBucket.kLow));

    const commentRow = historySection.querySelector('insights-comment-row');
    assertTrue(!!commentRow);
    assertTrue(isVisible(commentRow));

    const comment = commentRow.shadowRoot!.querySelector('#comment');
    assertTrue(!!comment);
    assertEquals(
        loadTimeData.getString('historyDescription'),
        comment.textContent!.trim());

    const feedbackButton =
        commentRow.shadowRoot!.querySelector('.link') as HTMLElement;
    assertTrue(!!feedbackButton);
    assertEquals(
        loadTimeData.getString('feedback'), feedbackButton.textContent!.trim());
    feedbackButton.click();
    assertEquals(1, shoppingListApi.getCallCount('showFeedback'));
    assertEquals(
        1, metrics.count('Commerce.PriceInsights.InlineFeedbackLinkClicked'));

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

    const titleSection =
        shoppingInsightsApp.shadowRoot!.querySelector('#titleSection');
    assertTrue(!!titleSection);
    assertFalse(
        isVisible(titleSection.querySelector('catalog-attributes-row')));
    assertTrue(isVisible(titleSection.querySelector('insights-comment-row')));

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

    const titleSection =
        shoppingInsightsApp.shadowRoot!.querySelector('#titleSection');
    assertTrue(!!titleSection);
    const attributesRow = titleSection.querySelector('catalog-attributes-row');
    assertTrue(!!attributesRow);
    assertTrue(isVisible(attributesRow));

    assertFalse(
        isVisible(attributesRow.shadowRoot!.querySelector('.attributes')));
    const buyOption =
        attributesRow.shadowRoot!.querySelector('.link') as HTMLElement;
    assertTrue(!!buyOption);
    assertEquals(
        loadTimeData.getString('buyOptions'), buyOption.textContent!.trim());
    buyOption.click();
    const url = await shoppingListApi.whenCalled('openUrlInNewTab');
    assertEquals('https://foo.com/jackpot', url.url);
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceInsights.BuyingOptionsClicked',
            PriceInsightsInfo_PriceBucket.kHigh));

    assertFalse(isVisible(titleSection.querySelector('insights-comment-row')));

    const historySection =
        shoppingInsightsApp.shadowRoot!.querySelector('#historySection');
    assertTrue(!!historySection);
    assertTrue(isVisible(historySection));

    const historyTitle =
        shoppingInsightsApp.shadowRoot!.querySelector('#historyTitle');
    assertTrue(!!historyTitle);
    assertEquals(
        loadTimeData.getString('historyTitleSingleOption'),
        historyTitle.textContent!.trim());
    assertFalse(
        isVisible(historySection.querySelector('catalog-attributes-row')));

    assertTrue(isVisible(historySection.querySelector('insights-comment-row')));

    assertTrue(isVisible(shoppingInsightsApp.shadowRoot!.querySelector(
        'shopping-insights-history-graph')));
  });

  test('EmptyJackpotLink', async () => {
    shoppingListApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights4}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await flushTasks();

    const titleSection =
        shoppingInsightsApp.shadowRoot!.querySelector('#titleSection');
    assertTrue(!!titleSection);
    const attributesRow = titleSection.querySelector('catalog-attributes-row');
    assertTrue(!!attributesRow);
    assertFalse(isVisible(attributesRow));
  });

  [true, false].forEach((eligible) => {
    test('PriceTrackingSectionVisibility', async () => {
      shoppingListApi.setResultFor(
          'isShoppingListEligible', Promise.resolve({eligible: eligible}));
      shoppingListApi.setResultFor(
          'getProductInfoForCurrentUrl',
          Promise.resolve({productInfo: productInfo}));
      shoppingListApi.setResultFor(
          'getPriceInsightsInfoForCurrentUrl',
          Promise.resolve({priceInsightsInfo: priceInsights1}));
      shoppingListApi.setResultFor(
          'getPriceTrackingStatusForCurrentUrl',
          Promise.resolve({tracked: true}));
      shoppingListApi.setResultFor(
          'getParentBookmarkFolderNameForCurrentUrl',
          Promise.resolve({name: stringToMojoString16('Parent folder')}));

      const callbackRouter = new PageCallbackRouter();
      shoppingListApi.setResultFor('getCallbackRouter', callbackRouter);

      document.body.appendChild(shoppingInsightsApp);
      await shoppingListApi.whenCalled('getProductInfoForCurrentUrl');
      await shoppingListApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
      await shoppingListApi.whenCalled('isShoppingListEligible');
      await flushTasks();

      const section = shoppingInsightsApp.shadowRoot!.querySelector(
                          '#priceTrackingSection') as PriceTrackingSection;
      assertEquals(isVisible(section), eligible);
      if (eligible) {
        assertEquals(section.priceInsightsInfo, priceInsights1);
        assertEquals(section.productInfo, productInfo);
      }
    });
  });
});
