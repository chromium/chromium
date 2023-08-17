// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shopping-insights-side-panel.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {PriceTrackingSection} from 'chrome://shopping-insights-side-panel.top-chrome/price_tracking_section.js';
import {ShoppingListApiProxyImpl} from 'chrome://shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo, PageCallbackRouter, PageRemote, PriceInsightsInfo, PriceInsightsInfo_PriceBucket, ProductInfo} from 'chrome://shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('PriceTrackingSectionTest', () => {
  let priceTrackingSection: PriceTrackingSection;
  let callbackRouter: PageCallbackRouter;
  let callbackRouterRemote: PageRemote;
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

  const priceInsights: PriceInsightsInfo = {
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

  const bookmarkProductInfo: BookmarkProductInfo = {
    bookmarkId: BigInt(3),
    info: productInfo,
  };

  function checkPriceTrackingSectionRendering(tracked: boolean) {
    assertEquals(
        priceTrackingSection.$.toggleTitle!.textContent,
        loadTimeData.getString('trackPriceTitle'));
    const expectedAnnotation = tracked ?
        (loadTimeData.getStringF('trackPriceDone', 'Parent folder') + '.') :
        (loadTimeData.getString('trackPriceDescription') + '.');
    assertEquals(
        priceTrackingSection.$.toggleAnnotation!.textContent,
        expectedAnnotation);
    assertEquals(
        priceTrackingSection.$.toggle!.getAttribute('aria-pressed')!,
        tracked ? 'true' : 'false');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    shoppingListApi.reset();
    callbackRouter = new PageCallbackRouter();
    shoppingListApi.setResultFor('getCallbackRouter', callbackRouter);
    shoppingListApi.setResultFor(
        'getParentBookmarkFolderNameForCurrentUrl',
        Promise.resolve({name: stringToMojoString16('Parent folder')}));

    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

    priceTrackingSection = document.createElement('price-tracking-section');
    priceTrackingSection.productInfo = productInfo;
    priceTrackingSection.priceInsightsInfo = priceInsights;

    metrics = fakeMetricsPrivate();
  });

  [true, false].forEach((tracked) => {
    test(
        `PriceTracking section rendering when tracked is ${tracked}`,
        async () => {
          shoppingListApi.setResultFor(
              'getPriceTrackingStatusForCurrentUrl',
              Promise.resolve({tracked: tracked}));

          document.body.appendChild(priceTrackingSection);
          await shoppingListApi.whenCalled(
              'getPriceTrackingStatusForCurrentUrl');
          await flushTasks();

          checkPriceTrackingSectionRendering(tracked);
        });

    test(`Toggle price tracking when tracked is ${tracked}`, async () => {
      shoppingListApi.setResultFor(
          'getPriceTrackingStatusForCurrentUrl',
          Promise.resolve({tracked: tracked}));

      document.body.appendChild(priceTrackingSection);
      await shoppingListApi.whenCalled('getPriceTrackingStatusForCurrentUrl');
      await flushTasks();

      priceTrackingSection.$.toggle!.click();

      const tracking = await shoppingListApi.whenCalled(
          'setPriceTrackingStatusForCurrentUrl');
      assertEquals(!tracking, tracked);
      if (tracking) {
        assertEquals(
            1,
            metrics.count(
                'Commerce.PriceTracking.PriceInsightsSidePanel.Track',
                PriceInsightsInfo_PriceBucket.kLow));
      } else {
        assertEquals(
            1,
            metrics.count(
                'Commerce.PriceTracking.PriceInsightsSidePanel.Untrack',
                PriceInsightsInfo_PriceBucket.kLow));
      }
    });

    test(`Ignore unrealted product tracking status change`, async () => {
      shoppingListApi.setResultFor(
          'getPriceTrackingStatusForCurrentUrl',
          Promise.resolve({tracked: tracked}));

      document.body.appendChild(priceTrackingSection);
      await shoppingListApi.whenCalled('getPriceTrackingStatusForCurrentUrl');
      await flushTasks();

      // Create a unrelated product.
      const otherProductInfo: ProductInfo = {
        title: 'Product Bar',
        clusterTitle: 'Product Cluster Bar',
        domain: 'bar.com',
        imageUrl: {url: 'https://bar.com/image'},
        productUrl: {url: 'https://bar.com/product'},
        currentPrice: '$12',
        previousPrice: '$34',
        clusterId: BigInt(54321),
      };

      const otherBookmarkProductInfo: BookmarkProductInfo = {
        bookmarkId: BigInt(4),
        info: otherProductInfo,
      };

      if (tracked) {
        callbackRouterRemote.priceUntrackedForBookmark(
            otherBookmarkProductInfo);
      } else {
        callbackRouterRemote.priceTrackedForBookmark(otherBookmarkProductInfo);
      }
      await flushTasks();
      checkPriceTrackingSectionRendering(tracked);
    });
  });

  test(`Observe current product tracking status change`, async () => {
    shoppingListApi.setResultFor(
        'getPriceTrackingStatusForCurrentUrl',
        Promise.resolve({tracked: false}));

    document.body.appendChild(priceTrackingSection);
    await shoppingListApi.whenCalled('getPriceTrackingStatusForCurrentUrl');
    await flushTasks();

    callbackRouterRemote.priceTrackedForBookmark(bookmarkProductInfo);
    await flushTasks();
    checkPriceTrackingSectionRendering(true);

    callbackRouterRemote.priceUntrackedForBookmark(bookmarkProductInfo);
    await flushTasks();
    checkPriceTrackingSectionRendering(false);
  });

  test(`Trigger bookmark editor`, async () => {
    shoppingListApi.setResultFor(
        'getPriceTrackingStatusForCurrentUrl',
        Promise.resolve({tracked: true}));

    document.body.appendChild(priceTrackingSection);
    await shoppingListApi.whenCalled('getPriceTrackingStatusForCurrentUrl');
    await flushTasks();
    checkPriceTrackingSectionRendering(true);

    const folder = priceTrackingSection.shadowRoot!.querySelector(
                       '#toggleAnnotationButton')! as HTMLElement;
    folder.click();

    await shoppingListApi.whenCalled('showBookmarkEditorForCurrentUrl');
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceTracking.' +
            'EditedBookmarkFolderFromPriceInsightsSidePanel'));
  });

  test(`Render error message`, async () => {
    shoppingListApi.setResultFor(
        'getPriceTrackingStatusForCurrentUrl',
        Promise.resolve({tracked: false}));

    document.body.appendChild(priceTrackingSection);
    await shoppingListApi.whenCalled('getPriceTrackingStatusForCurrentUrl');
    await flushTasks();

    callbackRouterRemote.operationFailedForBookmark(bookmarkProductInfo, true);
    await flushTasks();

    assertEquals(
        priceTrackingSection.$.toggleTitle!.textContent,
        loadTimeData.getString('trackPriceTitle'));
    assertEquals(
        priceTrackingSection.$.toggleAnnotation!.textContent,
        loadTimeData.getString('trackPriceError') + '.');
    assertEquals(
        priceTrackingSection.$.toggle!.getAttribute('aria-pressed'), 'false');

    callbackRouterRemote.operationFailedForBookmark(bookmarkProductInfo, false);
    await flushTasks();

    assertEquals(
        priceTrackingSection.$.toggleTitle!.textContent,
        loadTimeData.getString('trackPriceTitle'));
    assertEquals(
        priceTrackingSection.$.toggleAnnotation!.textContent,
        loadTimeData.getString('trackPriceError') + '.');
    assertEquals(
        priceTrackingSection.$.toggle!.getAttribute('aria-pressed'), 'true');
  });
});
