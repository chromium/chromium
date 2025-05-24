// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProductInfo} from 'chrome://resources/cr_components/commerce/shared.mojom-webui.js';
import type {PriceInsightsInfo, ProductSpecifications, UserFeedback} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PriceInsightsInfo_PriceBucket} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {ShoppingServiceBrowserProxy} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBrowserProxy extends BaseTestBrowserProxy implements
    ShoppingServiceBrowserProxy {
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
    priceSummary: '',
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

  constructor() {
    super([
      'getProductInfoForCurrentUrl',
      'getPriceInsightsInfoForCurrentUrl',
      'getUrlInfosForProductTabs',
      'getUrlInfosForRecentlyViewedTabs',
      'openUrlInNewTab',
      'switchToOrOpenTab',
      'isShoppingListEligible',
      'getPriceTrackingStatusForCurrentUrl',
      'getPriceInsightsInfoForUrl',
      'getProductInfoForUrl',
      'getProductInfoForUrls',
      'getProductSpecificationsForUrls',
      'getAllProductSpecificationsSets',
      'getProductSpecificationsSetByUuid',
      'addProductSpecificationsSet',
      'deleteProductSpecificationsSet',
      'setNameForProductSpecificationsSet',
      'setUrlsForProductSpecificationsSet',
      'setProductSpecificationsUserFeedback',
      'getProductSpecificationsFeatureState',
    ]);
  }

  getPriceInsightsInfoForUrl(url: Url) {
    this.methodCalled('getPriceInsightsInfoForUrl', url);
    return Promise.resolve({priceInsightsInfo: this.priceInsights_});
  }

  getProductInfoForUrl(url: Url) {
    this.methodCalled('getProductInfoForUrl', url);
    return Promise.resolve({productInfo: this.product_});
  }

  getProductInfoForUrls(urls: Url[]) {
    this.methodCalled('getProductInfoForUrls', urls);
    return Promise.resolve({productInfos: [this.product_]});
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

  openUrlInNewTab() {
    this.methodCalled('openUrlInNewTab');
  }

  switchToOrOpenTab() {
    this.methodCalled('switchToOrOpenTab');
  }

  isShoppingListEligible() {
    this.methodCalled('isShoppingListEligible');
    return Promise.resolve({eligible: false});
  }

  getPriceTrackingStatusForCurrentUrl() {
    this.methodCalled('getPriceTrackingStatusForCurrentUrl');
    return Promise.resolve({tracked: false});
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

  getProductSpecificationsFeatureState() {
    this.methodCalled('getProductSpecificationsFeatureState');
    return Promise.resolve({state: null});
  }
}
