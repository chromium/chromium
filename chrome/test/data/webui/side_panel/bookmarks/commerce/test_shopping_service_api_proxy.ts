// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {BookmarkProductInfo, PageRemote, PriceInsightsInfo, ProductInfo, ProductSpecifications} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PageCallbackRouter, PriceInsightsInfo_PriceBucket} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBrowserProxy extends BaseTestBrowserProxy implements
    BrowserProxy {
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
  private productSpecs_: ProductSpecifications = {
    products: [],
    productDimensionMap: new Map<bigint, string>(),
  };
  private shoppingCollectionId_: bigint = BigInt(-1);

  constructor() {
    super([
      'getAllPriceTrackedBookmarkProductInfo',
      'getAllShoppingBookmarkProductInfo',
      'trackPriceForBookmark',
      'untrackPriceForBookmark',
      'getProductInfoForCurrentUrl',
      'getPriceInsightsInfoForCurrentUrl',
      'getUrlInfosForOpenTabs',
      'getUrlInfosForRecentlyViewedTabs',
      'showInsightsSidePanelUi',
      'openUrlInNewTab',
      'showFeedback',
      'isShoppingListEligible',
      'getShoppingCollectionBookmarkFolderId',
      'getPriceTrackingStatusForCurrentUrl',
      'setPriceTrackingStatusForCurrentUrl',
      'getParentBookmarkFolderNameForCurrentUrl',
      'showBookmarkEditorForCurrentUrl',
      'getProductInfoForUrl',
      'getProductSpecificationsForUrls',
      'getAllProductSpecificationsSets',
      'getProductSpecificationsSetByUuid',
      'addProductSpecificationsSet',
      'deleteProductSpecificationsSet',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  setProducts(products: BookmarkProductInfo[]) {
    this.products_ = products;
  }

  setShoppingCollectionBookmarkFolderId(id: bigint) {
    this.shoppingCollectionId_ = id;
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

  getProductInfoForUrl(url: Url) {
    this.methodCalled('getProductInfoForUrl', url);
    return Promise.resolve({productInfo: this.product_});
  }

  getProductSpecificationsForUrls(urls: Url[]) {
    this.methodCalled('getProductSpecificationsForUrls', urls);
    return Promise.resolve({productSpecs: this.productSpecs_});
  }

  getProductInfoForCurrentUrl() {
    this.methodCalled('getProductInfoForCurrentUrl');
    return Promise.resolve({productInfo: this.product_});
  }

  getPriceInsightsInfoForCurrentUrl() {
    this.methodCalled('getPriceInsightsInfoForCurrentUrl');
    return Promise.resolve({priceInsightsInfo: this.priceInsights_});
  }

  getUrlInfosForOpenTabs() {
    this.methodCalled('getUrlInfosForOpenTabs');
    return Promise.resolve({urlInfos: []});
  }

  getUrlInfosForRecentlyViewedTabs() {
    this.methodCalled('getUrlInfosForRecentlyVisitedTabs');
    return Promise.resolve({urlInfos: []});
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
    this.methodCalled('isShoppingListEligible');
    return Promise.resolve({eligible: false});
  }

  getShoppingCollectionBookmarkFolderId() {
    this.methodCalled('getShoppingCollectionBookmarkFolderId');
    return Promise.resolve({collectionId: this.shoppingCollectionId_});
  }

  getPriceTrackingStatusForCurrentUrl() {
    this.methodCalled('getPriceTrackingStatusForCurrentUrl');
    return Promise.resolve({tracked: false});
  }

  setPriceTrackingStatusForCurrentUrl(track: boolean) {
    this.methodCalled('setPriceTrackingStatusForCurrentUrl', track);
  }

  getParentBookmarkFolderNameForCurrentUrl() {
    this.methodCalled('getParentBookmarkFolderNameForCurrentUrl');
    return Promise.resolve({name: {data: []}});
  }

  showBookmarkEditorForCurrentUrl() {
    this.methodCalled('showBookmarkEditorForCurrentUrl');
  }

  getAllProductSpecificationsSets() {
    this.methodCalled('getAllProductSpecificationsSets');
    return Promise.resolve({sets: []});
  }

  getProductSpecificationsSetByUuid(uuid: Uuid) {
    this.methodCalled('getProductSpecificationsSetByUuid', uuid);
    return Promise.resolve({set: null});
  }

  addProductSpecificationsSet(name: string, urls: Url[]) {
    this.methodCalled('addProductSpecificationsSet', name, urls);
    return Promise.resolve({createdSet: null});
  }

  deleteProductSpecificationsSet(uuid: Uuid) {
    this.methodCalled('deleteProductSpecificationsSet', uuid);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }
}
