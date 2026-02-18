// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProductInfo} from 'chrome://resources/cr_components/commerce/shared.mojom-webui.js';
import type {PriceInsightsInfo} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PriceInsightsInfo_PriceBucket} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {ShoppingServiceBrowserProxy} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBrowserProxy extends BaseTestBrowserProxy implements
    ShoppingServiceBrowserProxy {
  private product_: ProductInfo = {
    title: '',
    clusterTitle: '',
    domain: '',
    imageUrl: '',
    productUrl: '',
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
    jackpot: '',
    bucket: PriceInsightsInfo_PriceBucket.kUnknown,
    hasMultipleCatalogs: false,
    history: [],
    locale: '',
    currencyCode: '',
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
}
