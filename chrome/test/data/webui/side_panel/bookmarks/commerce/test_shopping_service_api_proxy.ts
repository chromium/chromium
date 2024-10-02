// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {BookmarkProductInfo, PageRemote, PriceInsightsInfo, ProductInfo, ProductSpecifications, ProductSpecificationsDisclosureVersion, UserFeedback} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
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
    categoryLabels: [],
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
      'getUrlInfosForProductTabs',
      'getUrlInfosForRecentlyViewedTabs',
      'showInsightsSidePanelUi',
      'openUrlInNewTab',
      'switchToOrOpenTab',
      'showFeedbackForPriceInsights',
      'isShoppingListEligible',
      'getShoppingCollectionBookmarkFolderId',
      'getPriceTrackingStatusForCurrentUrl',
      'setPriceTrackingStatusForCurrentUrl',
      'getParentBookmarkFolderNameForCurrentUrl',
      'showBookmarkEditorForCurrentUrl',
      'showProductSpecificationsSetForUuid',
      'getPriceInsightsInfoForUrl',
      'getProductInfoForUrl',
      'getProductSpecificationsForUrls',
      'getAllProductSpecificationsSets',
      'getProductSpecificationsSetByUuid',
      'addProductSpecificationsSet',
      'deleteProductSpecificationsSet',
      'setNameForProductSpecificationsSet',
      'setUrlsForProductSpecificationsSet',
      'setProductSpecificationsUserFeedback',
      'setProductSpecificationDisclosureAcceptVersion',
      'maybeShowProductSpecificationDisclosure',
      'declineProductSpecificationDisclosure',
      'showSyncSetupFlow',
      'getProductSpecificationsFeatureState',
      'getPageTitleFromHistory',
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

  getPriceInsightsInfoForUrl(url: Url) {
    this.methodCalled('getPriceInsightsInfoForUrl', url);
    return Promise.resolve({priceInsightsInfo: this.priceInsights_});
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

  getUrlInfosForProductTabs() {
    this.methodCalled('getUrlInfosForProductTabs');
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

  switchToOrOpenTab() {
    this.methodCalled('switchToOrOpenTab');
  }

  showFeedbackForPriceInsights() {
    this.methodCalled('showFeedbackForPriceInsights');
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

  showProductSpecificationsSetForUuid(uuid: Uuid, inNewTab: boolean) {
    this.methodCalled('showProductSpecificationsSetForUuid', uuid, inNewTab);
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

  setNameForProductSpecificationsSet(uuid: Uuid, name: string) {
    this.methodCalled('setNameForProductSpecificationsSet', uuid, name);
    return Promise.resolve({updatedSet: null});
  }

  setUrlsForProductSpecificationsSet(uuid: Uuid, urls: Url[]) {
    this.methodCalled('setUrlsForProductSpecificationsSet', uuid, urls);
    return Promise.resolve({updatedSet: null});
  }

  setProductSpecificationsUserFeedback(feedback: UserFeedback) {
    this.methodCalled('setUrlsForProductSpecificationsSet', feedback);
  }

  setProductSpecificationDisclosureAcceptVersion(
      version: ProductSpecificationsDisclosureVersion) {
    this.methodCalled(
        'setProductSpecificationDisclosureAcceptVersion', version);
  }

  maybeShowProductSpecificationDisclosure(
      urls: Url[], name: string, setId: string) {
    this.methodCalled(
        'maybeShowProductSpecificationDisclosure', urls, name, setId);
    return Promise.resolve({disclosureShown: false});
  }

  declineProductSpecificationDisclosure() {
    this.methodCalled('declineProductSpecificationDisclosure');
  }

  showSyncSetupFlow() {
    this.methodCalled('showSyncSetupFlow');
  }

  getProductSpecificationsFeatureState() {
    this.methodCalled('getProductSpecificationsFeatureState');
    return Promise.resolve({state: null});
  }

  getPageTitleFromHistory() {
    this.methodCalled('getPageTitleFromHistory');
    return Promise.resolve({title: ''});
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }
}
