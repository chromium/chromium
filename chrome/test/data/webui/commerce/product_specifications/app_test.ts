// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/app.js';

import type {ProductSpecificationsElement} from 'chrome://compare/app.js';
import {Router} from 'chrome://compare/router.js';
import type {ProductInfo, ProductSpecifications, ProductSpecificationsProduct} from 'chrome://compare/shopping_service.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {$$, assertNotStyle, assertStyle} from './test_support.js';

function createInfo(overrides?: Partial<ProductInfo>): ProductInfo {
  return Object.assign(
      {
        clusterId: BigInt(0),
        title: '',
        imageUrl: {url: ''},
        clusterTitle: '',
        domain: '',
        productUrl: {url: ''},
        currentPrice: '',
        previousPrice: '',
      },
      overrides);
}

function createSpecsProduct(overrides?: Partial<ProductSpecificationsProduct>):
    ProductSpecificationsProduct {
  return Object.assign(
      {
        productClusterId: BigInt(0),
        title: '',
        imageUrl: {url: ''},
        productDimensionValues: new Map<bigint, string[]>(),
        summary: '',
      },
      overrides);
}

function createSpecs(overrides?: Partial<ProductSpecifications>):
    ProductSpecifications {
  return Object.assign(
      {
        productDimensionMap: new Map<bigint, string>(),
        products: [createSpecsProduct()],
      },
      overrides);
}

interface AppPromiseValues {
  urls: string[];
  specs: ProductSpecifications;
  infos: ProductInfo[];
}

function createAppPromiseValues(overrides?: Partial<AppPromiseValues>):
    AppPromiseValues {
  return Object.assign(
      {
        urls: ['https://example.com/', 'https://example2.com/'],
        specs: createSpecs(),
        infos: [createInfo()],
      },
      overrides);
}

suite('AppTest', () => {
  let appElement: ProductSpecificationsElement;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);
  const router = TestMock.fromClass(Router);

  async function createAppElement(): Promise<ProductSpecificationsElement> {
    appElement = document.createElement('product-specifications-app');
    document.body.appendChild(appElement);
    return appElement;
  }

  async function createAppElementWithPromiseValues(
      promiseValues: AppPromiseValues =
          createAppPromiseValues()): Promise<ProductSpecificationsElement> {
    if (promiseValues.urls) {
      router.setResultFor(
          'getCurrentQuery', `?urls=${JSON.stringify(promiseValues.urls)}`);
    }
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsForUrls',
        Promise.resolve({productSpecs: promiseValues.specs}));
    let infoFetchCount = 0;
    shoppingServiceApi.setResultMapperFor('getProductInfoForUrl', () => {
      if (infoFetchCount >= promiseValues.infos.length) {
        return;
      }
      infoFetchCount++;
      return Promise.resolve(
          {productInfo: promiseValues.infos[infoFetchCount - 1]});
    });
    return createAppElement();
  }

  setup(async () => {
    shoppingServiceApi.reset();
    router.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
    Router.setInstance(router);
  });

  test('calls shopping service if there are url params', () => {
    createAppElementWithPromiseValues();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
  });

  test('handles invalid route', () => {
    router.setResultFor('getCurrentQuery', `?urls=INVALID_JSON}`);
    createAppElement();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        0, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
  });

  test('handles missing router', () => {
    router.setResultFor('getCurrentQuery', '');
    createAppElement();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        0, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
  });

  test('parses product urls', async () => {
    const testUrls = ['https://example.com/', 'https://example2.com/'];
    const promiseValues = createAppPromiseValues({urls: testUrls});
    createAppElementWithPromiseValues(promiseValues);

    const urls =
        await shoppingServiceApi.whenCalled('getProductSpecificationsForUrls');
    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertTrue(Array.isArray(urls));
    assertEquals(2, urls.length);
    assertArrayEquals(testUrls, urls.map(u => u.url));
  });

  test('populates specs table', async () => {
    const rowTitle = 'foo';
    const dimensionValues = ['bar', 'baz'];
    const dimensionValuesMap = new Map<bigint, string[]>(
        [[BigInt(2), dimensionValues], [BigInt(0), []]]);
    const specsProduct1 = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'qux',
      productDimensionValues: dimensionValuesMap,
    });
    const info1 = createInfo({
      clusterId: BigInt(123),
      title: 'qux',
      imageUrl: {url: 'qux.com/image'},
    });
    const info2 = createInfo({
      clusterId: BigInt(231),
      title: 'foobar',
      imageUrl: {url: 'foobar.com/image'},
    });

    const promiseValues = createAppPromiseValues({
      urls: ['https://example.com/', 'https://example2.com/'],
      specs: createSpecs({
        productDimensionMap: new Map<bigint, string>([[BigInt(2), rowTitle]]),
        products: [specsProduct1],
      }),
      infos: [info1, info2, createInfo({clusterId: BigInt(0)})],
    });
    createAppElementWithPromiseValues(promiseValues);
    await flushTasks();

    const columns = appElement.$.summaryTable.columns;
    assertEquals(2, columns.length);
    assertArrayEquals(
        [
          {
            selectedItem: {
              title: specsProduct1.title,
              url: 'https://example.com',
              imageUrl: info1.imageUrl.url,
            },
          },
          {
            selectedItem: {
              title: '',
              url: 'https://example.com',
              imageUrl: info2.imageUrl.url,
            },
          },
        ],
        columns);
    // Since only one of the two product dimension values has an ID, only
    // one row should be created.
    const rows = appElement.$.summaryTable.rows;
    assertEquals(1, rows.length);
    assertArrayEquals(
        [{title: rowTitle, values: [dimensionValues.join(',')]}], rows);
  });

  suite('EmptyState', () => {
    test('shows empty state if app loads without urls', () => {
      router.setResultFor('getCurrentQuery', '');
      createAppElement();

      assertNotStyle($$(appElement, '#empty')!, 'display', 'none');
      assertStyle($$(appElement, '#specs')!, 'display', 'none');
    });

    test('hides empty state if app loads with urls', () => {
      createAppElementWithPromiseValues();

      assertStyle($$(appElement, '#empty')!, 'display', 'none');
      assertNotStyle($$(appElement, '#specs')!, 'display', 'none');
    });
  });
});
