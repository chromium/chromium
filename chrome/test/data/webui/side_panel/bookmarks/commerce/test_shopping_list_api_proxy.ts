// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ShoppingListApiProxy} from 'chrome://bookmarks-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo, PageCallbackRouter, PageRemote, PriceInsightsInfo, PriceInsightsInfo_PriceBucket, ProductInfo} from 'chrome://bookmarks-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestShoppingListApiProxy extends TestBrowserProxy implements
    ShoppingListApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  private products_: BookmarkProductInfo[] = [];
  private product_: ProductInfo = {
    title: '',
    clusterTitle: '',
    domain: '',
    imageUrl: {url: ''},
    productUrl: {url: ''},
    currentPrice: '',
    previousPrice: '',
    clusterId: BigInt(0),
  };
  private priceInsights_: PriceInsightsInfo = {
    clusterId: BigInt(0),
    typicalLowPrice: '',
    typicalHighPrice: '',
    catalogAttributes: '',
    jackpot: {url: ''},
    bucket: PriceInsightsInfo_PriceBucket.kUnknown,
    hasMultipleCatalogs: false,
    history: [],
    locale: '',
    currencyCode: '',
  };

  constructor() {
    super([
      'getAllPriceTrackedBookmarkProductInfo',
      'getAllShoppingBookmarkProductInfo',
      'trackPriceForBookmark',
      'untrackPriceForBookmark',
      'getProductInfoForCurrentUrl',
      'getPriceInsightsInfoForCurrentUrl',
      'showInsightsSidePanelUi',
      'openUrlInNewTab',
      'showFeedback',
      'isShoppingListEligible',
      'getPriceTrackingStatusForCurrentUrl',
      'setPriceTrackingStatusForCurrentUrl',
      'getParentBookmarkFolderNameForCurrentUrl',
      'showBookmarkEditorForCurrentUrl',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  setProducts(products: BookmarkProductInfo[]) {
    this.products_ = products;
  }

  getAllPriceTrackedBookmarkProductInfo() {
    this.methodCalled('getAllPriceTrackedBookmarkProductInfo');
    return Promise.resolve({productInfos: this.products_});
  }

  getAllShoppingBookmarkProductInfo() {
    this.methodCalled('getAllShoppingBookmarkProductInfo');
    return Promise.resolve({productInfos: this.products_});
  }

  trackPriceForBookmark(bookmarkId: bigint) {
    this.methodCalled('trackPriceForBookmark', bookmarkId);
  }

  untrackPriceForBookmark(bookmarkId: bigint) {
    this.methodCalled('untrackPriceForBookmark', bookmarkId);
  }

  getProductInfoForCurrentUrl() {
    this.methodCalled('getProductInfoForCurrentUrl');
    return Promise.resolve({productInfo: this.product_});
  }

  getPriceInsightsInfoForCurrentUrl() {
    this.methodCalled('getPriceInsightsInfoForCurrentUrl');
    return Promise.resolve({priceInsightsInfo: this.priceInsights_});
  }

  showInsightsSidePanelUi() {
    this.methodCalled('showInsightsSidePanelUi');
  }

  openUrlInNewTab() {
    this.methodCalled('openUrlInNewTab');
  }

  showFeedback() {
    this.methodCalled('showFeedback');
  }

  isShoppingListEligible() {
    return this.methodCalled('isShoppingListEligible');
  }

  getPriceTrackingStatusForCurrentUrl() {
    return this.methodCalled('getPriceTrackingStatusForCurrentUrl');
  }

  setPriceTrackingStatusForCurrentUrl(track: boolean) {
    this.methodCalled('setPriceTrackingStatusForCurrentUrl', track);
  }

  getParentBookmarkFolderNameForCurrentUrl() {
    return this.methodCalled('getParentBookmarkFolderNameForCurrentUrl');
  }

  showBookmarkEditorForCurrentUrl() {
    this.methodCalled('showBookmarkEditorForCurrentUrl');
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }
}
