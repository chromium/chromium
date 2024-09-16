// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shopping-insights-side-panel.top-chrome/app.js';

import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {PriceInsightsInfo, ProductInfo} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PageCallbackRouter, PriceInsightsInfo_PriceBucket} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import type {ShoppingInsightsAppElement} from 'chrome://shopping-insights-side-panel.top-chrome/app.js';
import type {PriceTrackingSection} from 'chrome://shopping-insights-side-panel.top-chrome/price_tracking_section.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ShoppingInsightsAppTest', () => {
  let shoppingInsightsApp: ShoppingInsightsAppElement;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);
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
    categoryLabels: [],
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

    shoppingServiceApi.reset();
    shoppingServiceApi.setResultFor(
        'getProductInfoForCurrentUrl',
        Promise.resolve({productInfo: productInfo}));
    shoppingServiceApi.setResultFor(
        'isShoppingListEligible', Promise.resolve({eligible: false}));
    shoppingServiceApi.setResultFor(
        'getPriceTrackingStatusForCurrentUrl',
        Promise.resolve({tracked: false}));
    BrowserProxyImpl.setInstance(shoppingServiceApi);

    shoppingInsightsApp = document.createElement('shopping-insights-app');

    metrics = fakeMetricsPrivate();
  });

  test('HasBothRangeAndHistoryMultipleOptions', async () => {
    shoppingServiceApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights1}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingServiceApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingServiceApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
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

    const button = attributesRow.shadowRoot!.querySelector('cr-icon');
    assertTrue(!!button);
    button.click();
    const url = await shoppingServiceApi.whenCalled('openUrlInNewTab');
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
        commentRow.shadowRoot!.querySelector<HTMLElement>('.link');
    assertTrue(!!feedbackButton);
    assertEquals(
        loadTimeData.getString('feedback'), feedbackButton.textContent!.trim());
    feedbackButton.click();
    assertEquals(
        1, shoppingServiceApi.getCallCount('showFeedbackForPriceInsights'));
    assertEquals(
        1, metrics.count('Commerce.PriceInsights.InlineFeedbackLinkClicked'));

    assertTrue(isVisible(shoppingInsightsApp.shadowRoot!.querySelector(
        'shopping-insights-history-graph')));
  });

  test('HasRangeOnlySingleOption', async () => {
    shoppingServiceApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights2}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingServiceApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingServiceApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
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
    shoppingServiceApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights3}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingServiceApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingServiceApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
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
        attributesRow.shadowRoot!.querySelector<HTMLElement>('.link');
    assertTrue(!!buyOption);
    assertEquals(
        loadTimeData.getString('buyOptions'), buyOption.textContent!.trim());
    buyOption.click();
    const url = await shoppingServiceApi.whenCalled('openUrlInNewTab');
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
    shoppingServiceApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights4}));

    document.body.appendChild(shoppingInsightsApp);
    await shoppingServiceApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingServiceApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
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
      shoppingServiceApi.setResultFor(
          'isShoppingListEligible', Promise.resolve({eligible: eligible}));
      shoppingServiceApi.setResultFor(
          'getProductInfoForCurrentUrl',
          Promise.resolve({productInfo: productInfo}));
      shoppingServiceApi.setResultFor(
          'getPriceInsightsInfoForCurrentUrl',
          Promise.resolve({priceInsightsInfo: priceInsights1}));
      shoppingServiceApi.setResultFor(
          'getPriceTrackingStatusForCurrentUrl',
          Promise.resolve({tracked: true}));
      shoppingServiceApi.setResultFor(
          'getParentBookmarkFolderNameForCurrentUrl',
          Promise.resolve({name: stringToMojoString16('Parent folder')}));

      const callbackRouter = new PageCallbackRouter();
      shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);

      document.body.appendChild(shoppingInsightsApp);
      await shoppingServiceApi.whenCalled('getProductInfoForCurrentUrl');
      await shoppingServiceApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
      await shoppingServiceApi.whenCalled('isShoppingListEligible');
      await shoppingServiceApi.whenCalled(
          'getPriceTrackingStatusForCurrentUrl');
      await flushTasks();

      const section =
          shoppingInsightsApp.shadowRoot!.querySelector<PriceTrackingSection>(
              '#priceTrackingSection');
      assertEquals(isVisible(section), eligible);
      if (eligible) {
        assertTrue(!!section);
        assertEquals(section.priceInsightsInfo, priceInsights1);
        assertEquals(section.productInfo, productInfo);
      }
    });
  });

  test('NotShowPriceTrackingWithoutTrackingStatus', async () => {
    shoppingServiceApi.setResultFor(
        'isShoppingListEligible', Promise.resolve({eligible: true}));
    shoppingServiceApi.setResultFor(
        'getProductInfoForCurrentUrl',
        Promise.resolve({productInfo: productInfo}));
    shoppingServiceApi.setResultFor(
        'getPriceInsightsInfoForCurrentUrl',
        Promise.resolve({priceInsightsInfo: priceInsights1}));
    shoppingServiceApi.setResultFor(
        'getParentBookmarkFolderNameForCurrentUrl',
        Promise.resolve({name: stringToMojoString16('Parent folder')}));

    const callbackRouter = new PageCallbackRouter();
    shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);

    document.body.appendChild(shoppingInsightsApp);
    await shoppingServiceApi.whenCalled('getProductInfoForCurrentUrl');
    await shoppingServiceApi.whenCalled('getPriceInsightsInfoForCurrentUrl');
    await shoppingServiceApi.whenCalled('isShoppingListEligible');

    // Price tracking section is not visible before
    // `getPriceTrackingStatusForCurrentUrl` returns.
    let section =
        shoppingInsightsApp.shadowRoot!.querySelector<PriceTrackingSection>(
            '#priceTrackingSection');
    assertFalse(isVisible(section));

    await shoppingServiceApi.whenCalled('getPriceTrackingStatusForCurrentUrl');
    await flushTasks();

    section =
        shoppingInsightsApp.shadowRoot!.querySelector<PriceTrackingSection>(
            '#priceTrackingSection');
    assertTrue(isVisible(section));
  });
});
