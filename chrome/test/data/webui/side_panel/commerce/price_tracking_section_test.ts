// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://shopping-insights-side-panel.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PriceTrackingSection} from 'chrome://shopping-insights-side-panel.top-chrome/price_tracking_section.js';
import {ShoppingListApiProxyImpl} from 'chrome://shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo, PageCallbackRouter, PageRemote, ProductInfo} from 'chrome://shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('PriceTrackingSectionTest', () => {
  let priceTrackingSection: PriceTrackingSection;
  let callbackRouter: PageCallbackRouter;
  let callbackRouterRemote: PageRemote;
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

  const bookmarkProductInfo: BookmarkProductInfo = {
    bookmarkId: BigInt(3),
    info: productInfo,
  };

  function checkPriceTrackingSectionRendering(tracked: boolean) {
    assertEquals(
        priceTrackingSection.$.toggleTitle!.textContent,
        loadTimeData.getString('trackPriceTitle'));
    assertEquals(
        priceTrackingSection.$.toggleAnnotation!.textContent,
        tracked ? loadTimeData.getStringF('trackPriceDone', '') :
                  loadTimeData.getString('trackPriceDescription'));
    assertEquals(
        priceTrackingSection.$.toggle!.getAttribute('aria-pressed')!,
        tracked ? 'true' : 'false');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    shoppingListApi.reset();
    callbackRouter = new PageCallbackRouter();
    shoppingListApi.setResultFor('getCallbackRouter', callbackRouter);
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

    priceTrackingSection = document.createElement('price-tracking-section');
    priceTrackingSection.productInfo = productInfo;
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
});
