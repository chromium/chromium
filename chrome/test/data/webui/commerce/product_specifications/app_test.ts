// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/app.js';

import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {ProductSpecificationsElement} from 'chrome://compare/app.js';
import {Router} from 'chrome://compare/router.js';
import type {ProductInfo, ProductSpecifications, ProductSpecificationsProduct, ProductSpecificationsSet, ProductSpecificationsValue} from 'chrome://compare/shopping_service.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {PageCallbackRouter, UserFeedback} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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
        categoryLabels: [],
      },
      overrides);
}

function createSpecsProduct(overrides?: Partial<ProductSpecificationsProduct>):
    ProductSpecificationsProduct {
  return Object.assign(
      {
        productClusterId: BigInt(0),
        title: '',
        productUrl: {url: ''},
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

function createSpecsSet(overrides?: Partial<ProductSpecificationsSet>):
    ProductSpecificationsSet {
  return Object.assign(
      {
        name: '',
        uuid: {value: ''},
        urls: [],
      },
      overrides);
}

interface AppPromiseValues {
  idParam: string;
  urlsParam: string[];
  specs: ProductSpecifications;
  infos: ProductInfo[];
  specsSet: ProductSpecificationsSet|null;
}

function createAppPromiseValues(overrides?: Partial<AppPromiseValues>):
    AppPromiseValues {
  return Object.assign(
      {
        idParam: '',
        urlsParam: '',
        specs: createSpecs(),
        infos: [createInfo()],
        specsSet: null,
      },
      overrides);
}

suite('AppTest', () => {
  let appElement: ProductSpecificationsElement;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);
  const callbackRouter = new PageCallbackRouter();
  const callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
  const router = TestMock.fromClass(Router);

  async function createAppElement(): Promise<ProductSpecificationsElement> {
    appElement = document.createElement('product-specifications-app');
    document.body.appendChild(appElement);
    appElement.resetMinLoadingAnimationMsForTesting();
    return appElement;
  }

  async function createAppElementWithPromiseValues(
      promiseValues: AppPromiseValues =
          createAppPromiseValues()): Promise<ProductSpecificationsElement> {
    const params = new URLSearchParams();
    if (promiseValues.idParam) {
      params.append('id', promiseValues.idParam);
    }
    if (promiseValues.urlsParam && promiseValues.urlsParam.length > 0) {
      params.append('urls', JSON.stringify(promiseValues.urlsParam));
    }
    router.setResultFor('getCurrentQuery', params);
    shoppingServiceApi.setResultFor(
        'addProductSpecificationsSet',
        Promise.resolve({createdSet: promiseValues.specsSet}));
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsForUrls',
        Promise.resolve({productSpecs: promiseValues.specs}));
    shoppingServiceApi.setResultMapperFor(
        'getProductInfoForUrl', (url: Url) => {
          for (const info of promiseValues.infos) {
            if (info.productUrl.url === url.url) {
              return Promise.resolve({productInfo: info});
            }
          }
          const emptyInfo = createInfo();
          return Promise.resolve({productInfo: emptyInfo});
        });

    const appElement = await createAppElement();
    await flushTasks();

    return appElement;
  }

  setup(async () => {
    loadTimeData.overrideValues({priceRowTitle: 'price'});
    shoppingServiceApi.reset();
    shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);
    router.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
    Router.setInstance(router);
  });

  test('calls shopping service when there are url params', () => {
    const urlsParam = ['https://example.com/', 'https://example2.com/'];
    router.setResultFor(
        'getCurrentQuery',
        new URLSearchParams('urls=' + JSON.stringify(urlsParam)));
    createAppElement();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        1, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
    assertEquals(
        'Product specs',
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][0]);
    assertArrayEquals(
        urlsParam.map(url => ({url})),
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][1]);
  });

  test('handles invalid route', () => {
    router.setResultFor(
        'getCurrentQuery', new URLSearchParams('urls=INVALID_JSON'));
    createAppElement();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        0, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
  });

  test('handles missing router', () => {
    router.setResultFor('getCurrentQuery', new URLSearchParams(''));
    createAppElement();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        0, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
  });

  test('parses product urls', async () => {
    const urlsParam = ['https://example.com/', 'https://example2.com/'];
    const promiseValues = createAppPromiseValues({urlsParam: urlsParam});
    await createAppElementWithPromiseValues(promiseValues);

    const urls =
        await shoppingServiceApi.whenCalled('getProductSpecificationsForUrls');
    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertTrue(Array.isArray(urls));
    assertArrayEquals(urlsParam, urls.map(u => u.url));
  });

  test('prioritizes id param over urls param', async () => {
    const specsSetUrls =
        [{url: 'https://example.com/'}, {url: 'https://example2.com/'}];
    const testId = 'foo123';
    const specsSet =
        createSpecsSet({urls: specsSetUrls, uuid: {value: testId}});
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));
    const promiseValues = createAppPromiseValues({
      idParam: testId,
      urlsParam: ['https://example3.com/', 'https://example4.com/'],
      specsSet: specsSet,
    });
    await createAppElementWithPromiseValues(promiseValues);

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        1,
        shoppingServiceApi.getCallCount('getProductSpecificationsSetByUuid'));
    assertEquals(
        testId,
        shoppingServiceApi.getArgs('getProductSpecificationsSetByUuid')[0]
            .value);
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    // Ensure that urls came from the specs set and not the url parameter.
    assertArrayEquals(
        specsSetUrls,
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[0]);
    // Ensure that a specs set was not added for the `urls` search parameter.
    assertEquals(
        0, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
  });

  test('creates id for urls param', async () => {
    const urlsParam = ['https://example3.com/', 'https://example4.com/'];
    const promiseValues = createAppPromiseValues({
      idParam: '',
      urlsParam: urlsParam,
    });
    await createAppElementWithPromiseValues(promiseValues);

    assertEquals(
        1, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
    assertEquals(
        'Product specs',
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][0]);
    const mappedUrlsParams = urlsParam.map(url => ({url}));
    assertArrayEquals(
        mappedUrlsParams,
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][1]);
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertArrayEquals(
        mappedUrlsParams,
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[0]);
    assertEquals(
        0,
        shoppingServiceApi.getCallCount('getProductSpecificationsSetByUuid'));
  });

  test('populates specs table', async () => {
    const detailTitle = 'foo';
    const dimensionValues = {
      summary: [{
        text: 'summary',
        urls: [{
          url: {url: ''},
          title: '',
          faviconUrl: {url: ''},
          thumbnailUrl: {url: ''},
        }],
      }],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [
            {
              descriptions: [
                {
                  text: 'bar',
                  urls: [{
                    url: {url: ''},
                    title: '',
                    faviconUrl: {url: ''},
                    thumbnailUrl: {url: ''},
                  }],
                },
                {
                  text: 'baz',
                  urls: [{
                    url: {url: ''},
                    title: '',
                    faviconUrl: {url: ''},
                    thumbnailUrl: {url: ''},
                  }],
                },
              ],
            },
          ],
        },
      ],
    };
    const emptyValue = {summary: [], specificationDescriptions: []};
    const dimensionValuesMap = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues], [BigInt(0), emptyValue]]);
    const specsProduct1 = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'qux',
      productDimensionValues: dimensionValuesMap,
    });
    const info1 = createInfo({
      clusterId: BigInt(123),
      title: 'qux',
      productUrl: {url: 'https://example.com/'},
      imageUrl: {url: 'qux.com/image'},
      currentPrice: '$100',
    });
    const info2 = createInfo({
      clusterId: BigInt(231),
      title: 'foobar',
      productUrl: {url: 'https://example2.com/'},
      imageUrl: {url: 'foobar.com/image'},
    });

    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/', 'https://example2.com/'],
      specs: createSpecs({
        productDimensionMap:
            new Map<bigint, string>([[BigInt(2), detailTitle]]),
        products: [specsProduct1],
      }),
      infos: [info1, info2, createInfo({clusterId: BigInt(0)})],
    });
    await createAppElementWithPromiseValues(promiseValues);

    const tableColumns = appElement.$.summaryTable.columns;
    assertEquals(2, tableColumns.length);
    assertArrayEquals(
        [
          {
            selectedItem: {
              title: specsProduct1.title,
              url: 'https://example.com/',
              imageUrl: info1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', description: '$100', summary: ''},
              {title: detailTitle, description: 'bar, baz', summary: 'summary'},
            ],
          },
          {
            selectedItem: {
              // If the product spec doesn't have a title, the column should
              // use the title from the product info.
              title: info2.title,
              url: 'https://example2.com/',
              imageUrl: info2.imageUrl.url,
            },
            // Since this item's product dimension values have no ID, its
            // `productDetails` should have empty strings for `description` and
            // summary`.
            productDetails: [
              {title: 'price', description: '', summary: ''},
              {title: detailTitle, description: '', summary: ''},
            ],
          },
        ],
        tableColumns);
  });

  test('populates specs table, no summary', async () => {
    const detailTitle = 'foo';
    const dimensionValues = {
      summary: [],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [
            {
              descriptions: [
                {
                  text: 'bar',
                  urls: [{
                    url: {url: ''},
                    title: '',
                    faviconUrl: {url: ''},
                    thumbnailUrl: {url: ''},
                  }],
                },
              ],
            },
          ],
        },
      ],
    };
    const dimensionValuesMap = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues]]);
    const specsProduct1 = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'qux',
      productDimensionValues: dimensionValuesMap,
    });
    const info1 = createInfo({
      clusterId: BigInt(123),
      title: 'qux',
      productUrl: {url: 'https://example.com/'},
      imageUrl: {url: 'qux.com/image'},
    });

    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
      specs: createSpecs({
        productDimensionMap:
            new Map<bigint, string>([[BigInt(2), detailTitle]]),
        products: [specsProduct1],
      }),
      infos: [info1],
    });
    await createAppElementWithPromiseValues(promiseValues);

    const tableColumns = appElement.$.summaryTable.columns;
    assertEquals(1, tableColumns.length);
    assertArrayEquals(
        [
          {
            selectedItem: {
              title: specsProduct1.title,
              url: 'https://example.com/',
              imageUrl: info1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', description: '', summary: ''},
              {title: detailTitle, description: 'bar', summary: ''},
            ],
          },
        ],
        tableColumns);
  });

  test('populates specs table, correct column order', async () => {
    // Set up the first product with at least one unique description.
    const dimensionValues1 = {
      summary: [],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [
            {
              descriptions: [
                {
                  text: 'desc 1',
                  urls: [{
                    url: {url: ''},
                    title: '',
                    faviconUrl: {url: ''},
                    thumbnailUrl: {url: ''},
                  }],
                },
              ],
            },
          ],
        },
      ],
    };
    const dimensionValuesMap1 = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues1]]);
    const specsProduct1 = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'Product 1',
      productDimensionValues: dimensionValuesMap1,
    });
    const info1 = createInfo({
      clusterId: BigInt(123),
      title: 'Product 1',
      productUrl: {url: 'https://example.com/1'},
      imageUrl: {url: 'http://example.com/image1.png'},
    });

    // Set up the second product - the description needs to be different from
    // the one above.
    const dimensionValues2 = {
      summary: [],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [
            {
              descriptions: [
                {
                  text: 'desc 2',
                  urls: [{
                    url: {url: ''},
                    title: '',
                    faviconUrl: {url: ''},
                    thumbnailUrl: {url: ''},
                  }],
                },
              ],
            },
          ],
        },
      ],
    };
    const dimensionValuesMap2 = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues2]]);
    const specsProduct2 = createSpecsProduct({
      productClusterId: BigInt(456),
      title: 'Product 2',
      productDimensionValues: dimensionValuesMap2,
    });
    const info2 = createInfo({
      clusterId: BigInt(456),
      title: 'Product 2',
      productUrl: {url: 'https://example.com/2'},
      imageUrl: {url: 'http://example.com/image2.png'},
    });

    const detailTitle = 'Section';
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/1', 'https://example.com/2'],
      specs: createSpecs({
        productDimensionMap:
            new Map<bigint, string>([[BigInt(2), detailTitle]]),
        // These products are intentionally swapped to ensure they are shown
        // in the correct column, even if output order doesn't match input
        // order.
        products: [specsProduct2, specsProduct1],
      }),
      infos: [info1, info2],
    });
    await createAppElementWithPromiseValues(promiseValues);

    // Ensure the column header matches the content.
    const tableColumns = appElement.$.summaryTable.columns;
    assertEquals(2, tableColumns.length);
    assertArrayEquals(
        [
          {
            selectedItem: {
              title: specsProduct1.title,
              url: 'https://example.com/1',
              imageUrl: info1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', description: '', summary: ''},
              {title: detailTitle, description: 'desc 1', summary: ''},
            ],
          },
          {
            selectedItem: {
              title: specsProduct2.title,
              url: 'https://example.com/2',
              imageUrl: info2.imageUrl.url,
            },
            productDetails: [
              {title: 'price', description: '', summary: ''},
              {title: detailTitle, description: 'desc 2', summary: ''},
            ],
          },
        ],
        tableColumns);
  });

  test('reacts to update event, column reordering', async () => {
    // Set up the first product with at least one unique description.
    const dimensionValues1 = {
      summary: [],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [
            {
              descriptions: [
                {
                  text: 'desc 1',
                  urls: [{
                    url: {url: ''},
                    title: '',
                    faviconUrl: {url: ''},
                    thumbnailUrl: {url: ''},
                  }],
                },
              ],
            },
          ],
        },
      ],
    };
    const dimensionValuesMap1 = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues1]]);

    const specsProduct1 = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'Product 1',
      productDimensionValues: dimensionValuesMap1,
    });
    const info1 = createInfo({
      clusterId: BigInt(123),
      title: 'Product 1',
      productUrl: {url: 'https://example.com/1'},
      imageUrl: {url: 'http://example.com/image1.png'},
    });

    // Set up the second product - the description needs to be different from
    // the one above.
    const dimensionValues2 = {
      summary: [],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [
            {
              descriptions: [
                {
                  text: 'desc 2',
                  urls: [{
                    url: {url: ''},
                    title: '',
                    faviconUrl: {url: ''},
                    thumbnailUrl: {url: ''},
                  }],
                },
              ],
            },
          ],
        },
      ],
    };
    const dimensionValuesMap2 = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues2]]);
    const specsProduct2 = createSpecsProduct({
      productClusterId: BigInt(456),
      title: 'Product 2',
      productDimensionValues: dimensionValuesMap2,
    });
    const info2 = createInfo({
      clusterId: BigInt(456),
      title: 'Product 2',
      productUrl: {url: 'https://example.com/2'},
      imageUrl: {url: 'http://example.com/image2.png'},
    });

    const specsSetUrls =
        [{url: 'https://example.com/1'}, {url: 'https://example.com/2'}];
    const testId = '00000000-0000-0000-0000-000000000001';
    const specsSet =
        createSpecsSet({urls: specsSetUrls, uuid: {value: testId}});
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));

    const rowTitle = 'Section';
    const promiseValues = createAppPromiseValues({
      idParam: testId,
      specs: createSpecs({
        productDimensionMap: new Map<bigint, string>([[BigInt(2), rowTitle]]),
        products: [specsProduct1, specsProduct2],
      }),
      infos: [info1, info2],
    });
    await createAppElementWithPromiseValues(promiseValues);

    // We should only have a single call to the backend.
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertArrayEquals(
        [{url: 'https://example.com/1'}, {url: 'https://example.com/2'}],
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[0]);
    // Ensure column count and the first column URL is correct.
    let columns = appElement.$.summaryTable.columns;
    assertEquals(2, columns.length);
    assertEquals('https://example.com/1', columns[0]?.selectedItem.url);
    assertEquals('https://example.com/2', columns[1]?.selectedItem.url);

    // Trigger an update where the URLs haven't changed, they just change order.
    const orderSwitchedSpecsSetUrls =
        [{url: 'https://example.com/2'}, {url: 'https://example.com/1'}];
    callbackRouterRemote.onProductSpecificationsSetUpdated(createSpecsSet(
        {urls: orderSwitchedSpecsSetUrls, uuid: {value: testId}}));
    await waitAfterNextRender(appElement);

    // Since the URLs didn't change, there should still only have been a single
    // call to the backend.
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    // After the update, the columns should be swapped.
    columns = appElement.$.summaryTable.columns;
    assertEquals(2, columns.length);
    assertArrayEquals(
        [
          {
            selectedItem: {
              title: specsProduct2.title,
              url: 'https://example.com/2',
              imageUrl: info2.imageUrl.url,
            },
            productDetails: [
              {title: 'price', description: '', summary: ''},
              {title: rowTitle, description: 'desc 2', summary: ''},
            ],
          },
          {
            selectedItem: {
              title: specsProduct1.title,
              url: 'https://example.com/1',
              imageUrl: info1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', description: '', summary: ''},
              {title: rowTitle, description: 'desc 1', summary: ''},
            ],
          },
        ],
        columns);
  });

  test('reacts to update event, name only', async () => {
    // Set up the first product with at least one unique description.
    const dimensionValues = {
      summary: [],
      specificationDescriptions: [],
    };
    const dimensionValuesMap = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues]]);
    const specsProduct = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'Product',
      productDimensionValues: dimensionValuesMap,
    });
    const info = createInfo({
      clusterId: BigInt(123),
      title: 'Product',
      productUrl: {url: 'https://example.com/'},
      imageUrl: {url: 'http://example.com/image.png'},
    });
    const specsSetUrls = [{url: 'https://example.com/'}];
    const testId = '00000000-0000-0000-0000-000000000001';
    const specsSet =
        createSpecsSet({urls: specsSetUrls, uuid: {value: testId}});
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));
    const promiseValues = createAppPromiseValues({
      idParam: testId,
      specs: createSpecs({
        productDimensionMap:
            new Map<bigint, string>([[BigInt(2), 'Row Title']]),
        products: [specsProduct],
      }),
      infos: [info],
    });
    await createAppElementWithPromiseValues(promiseValues);

    // We should only have a single call to the backend.
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertArrayEquals(
        [{url: 'https://example.com/'}],
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[0]);

    // Trigger an update where only the title has changed.
    callbackRouterRemote.onProductSpecificationsSetUpdated(createSpecsSet(
        {name: 'Diff title', urls: specsSetUrls, uuid: {value: testId}}));
    await waitAfterNextRender(appElement);

    // Since the URLs didn't change, there should still only have been a single
    // call to the backend.
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
  });

  test('reacts to update event, url change', async () => {
    const dimensionValues = {
      summary: [],
      specificationDescriptions: [],
    };
    const dimensionValuesMap = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues]]);
    const specsProduct = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'Product',
      productDimensionValues: dimensionValuesMap,
    });
    const info = createInfo({
      clusterId: BigInt(123),
      title: 'Product',
      productUrl: {url: 'https://example.com/'},
      imageUrl: {url: 'http://example.com/image.png'},
    });
    const testId = '00000000-0000-0000-0000-000000000001';
    const specsSet = createSpecsSet(
        {urls: [{url: 'https://example.com/'}], uuid: {value: testId}});
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));

    const promiseValues = createAppPromiseValues({
      idParam: testId,
      specs: createSpecs({
        productDimensionMap:
            new Map<bigint, string>([[BigInt(2), 'Row Title']]),
        products: [specsProduct],
      }),
      infos: [info],
    });
    await createAppElementWithPromiseValues(promiseValues);

    // We should only have a single call to the backend.
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertArrayEquals(
        [{url: 'https://example.com/'}],
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[0]);

    // Trigger an update where only the title has changed.
    callbackRouterRemote.onProductSpecificationsSetUpdated(createSpecsSet(
        {urls: [{url: 'https://example.com/new_url'}], uuid: {value: testId}}));
    await waitAfterNextRender(appElement);

    // A URL change should trigger another call to the backend.
    assertEquals(
        2, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertArrayEquals(
        [{url: 'https://example.com/new_url'}],
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[1]);
  });

  test('adding url w/o existing set creates new set', async () => {
    const productTabs = [{
      title: 'title',
      url: stringToMojoUrl('https://example.com/'),
    }];
    shoppingServiceApi.setResultFor(
        'getUrlInfosForProductTabs', Promise.resolve({urlInfos: productTabs}));
    shoppingServiceApi.setResultFor(
        'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));
    createAppElement();

    // Click on the "add column" button and select the first (only) item.
    const newColSelector = appElement.$.newColumnSelector;
    newColSelector.$.button.click();
    await waitAfterNextRender(appElement);
    const menu = newColSelector.$.productSelectionMenu;
    const crActionMenu = menu.$.menu.get();
    assertTrue(crActionMenu.open);
    const dropdownItem =
        crActionMenu.querySelector<HTMLElement>('.dropdown-item')!;
    dropdownItem.click();
    await waitAfterNextRender(appElement);

    // Since the UI wasn't showing an existing set, we should attempt to
    // create one.
    const args =
        await shoppingServiceApi.whenCalled('addProductSpecificationsSet');
    assertEquals(2, args.length);
    assertArrayEquals([{url: 'https://example.com/'}], args[1]);
  });

  test('add url for existing set', async () => {
    const dimensionValues = {
      summary: [],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [],
        },
      ],
    };
    const dimensionValuesMap = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues]]);
    const specsProduct = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'Product',
      productDimensionValues: dimensionValuesMap,
    });
    const info = createInfo({
      clusterId: BigInt(123),
      title: 'Product',
      productUrl: {url: 'https://example.com/'},
      imageUrl: {url: 'http://example.com/image.png'},
    });
    const testId = '00000000-0000-0000-0000-000000000001';
    const promiseValues = createAppPromiseValues({
      idParam: testId,
      specs: createSpecs({
        productDimensionMap: new Map<bigint, string>([[BigInt(2), 'Title']]),
        products: [specsProduct],
      }),
      infos: [info],
    });
    const specsSet = createSpecsSet(
        {urls: [{url: 'https://example.com/'}], uuid: {value: testId}});
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));
    const productTabs = [{
      title: 'title 2',
      url: stringToMojoUrl('https://example.com/2'),
    }];
    shoppingServiceApi.setResultFor(
        'getUrlInfosForProductTabs', Promise.resolve({urlInfos: productTabs}));
    shoppingServiceApi.setResultFor(
        'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));
    await createAppElementWithPromiseValues(promiseValues);

    // Click on the "add column" button and select the first (only) item.
    const newColSelector = appElement.$.newColumnSelector;
    newColSelector.$.button.click();
    await waitAfterNextRender(appElement);
    const menu = newColSelector.$.productSelectionMenu;
    const crActionMenu = menu.$.menu.get();
    assertTrue(crActionMenu.open);
    const dropdownItem =
        crActionMenu.querySelector<HTMLElement>('.dropdown-item')!;
    dropdownItem.click();
    await waitAfterNextRender(appElement);

    // We should see a call to update the URLs in the set.
    const args = await shoppingServiceApi.whenCalled(
        'setUrlsForProductSpecificationsSet');
    assertEquals(2, args.length);
    assertArrayEquals(
        [{url: 'https://example.com/'}, {url: 'https://example.com/2'}],
        args[1]);
  });

  test('name change updates page title', async () => {
    const dimensionValues = {
      summary: [],
      specificationDescriptions: [
        {
          label: '',
          altText: '',
          options: [],
        },
      ],
    };
    const dimensionValuesMap = new Map<bigint, ProductSpecificationsValue>(
        [[BigInt(2), dimensionValues]]);
    const specsProduct = createSpecsProduct({
      productClusterId: BigInt(123),
      title: 'Product',
      productDimensionValues: dimensionValuesMap,
    });
    const info = createInfo({
      clusterId: BigInt(123),
      title: 'Product',
      productUrl: {url: 'https://example.com/'},
      imageUrl: {url: 'http://example.com/image.png'},
    });
    const testId = '00000000-0000-0000-0000-000000000001';
    const promiseValues = createAppPromiseValues({
      idParam: testId,
      specs: createSpecs({
        productDimensionMap: new Map<bigint, string>([[BigInt(2), 'Title']]),
        products: [specsProduct],
      }),
      infos: [info],
    });
    const specsSet = createSpecsSet({
      name: 'My products',
      urls: [{url: 'https://example.com/'}],
      uuid: {value: testId},
    });
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));
    await createAppElementWithPromiseValues(promiseValues);

    // Since we loaded an existing set, the page title should use the name of
    // the set.
    assertEquals('My products', document.title);

    // Simulate a name change from sync.
    callbackRouterRemote.onProductSpecificationsSetUpdated(createSpecsSet(
        {name: 'My specific products', urls: [], uuid: {value: testId}}));
    await flushTasks();

    // The name should have changed with the update event.
    assertEquals('My specific products', document.title);
  });

  test('shows full table loading state', async () => {
    const minLoadingAnimationMs = 10;
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
    });
    createAppElementWithPromiseValues(promiseValues);
    appElement.resetMinLoadingAnimationMsForTesting(minLoadingAnimationMs);
    await flushTasks();

    assertTrue(isVisible(appElement.$.loading));
    assertFalse(isVisible(appElement.$.summaryTable));

    // Wait for the loading animation to finish.
    await new Promise(res => setTimeout(res, minLoadingAnimationMs));

    assertFalse(isVisible(appElement.$.loading));
  });

  test('disables menu button while loading', async () => {
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
    });
    createAppElementWithPromiseValues(promiseValues);
    appElement.resetMinLoadingAnimationMsForTesting(500);
    await flushTasks();

    assertTrue(appElement.$.header.$.menuButton.disabled);
  });

  test('show feedback loading state while loading', async () => {
    const minLoadingAnimationMs = 10;
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
    });
    createAppElementWithPromiseValues(promiseValues);
    const feedbackButtonPlacholder =
        appElement.shadowRoot!.querySelector('#feedbackLoading');
    const feedbackButtons = appElement.$.feedbackButtons;
    appElement.resetMinLoadingAnimationMsForTesting(minLoadingAnimationMs);
    await flushTasks();

    assertTrue(isVisible(feedbackButtonPlacholder));
    assertFalse(isVisible(feedbackButtons));

    // Wait for the loading animation to finish.
    await new Promise(res => setTimeout(res, minLoadingAnimationMs));

    assertFalse(isVisible(feedbackButtonPlacholder));
    assertTrue(isVisible(feedbackButtons));
  });

  test('updates on selection change', async () => {
    const urlsParam = ['https://example.com/', 'https://example2.com/'];
    const specsSetUrls =
        [{url: 'https://example.com/'}, {url: 'https://example2.com/'}];
    const specsSet = createSpecsSet({urls: specsSetUrls, uuid: {value: 'foo'}});
    const promiseValues =
        createAppPromiseValues({urlsParam: urlsParam, specsSet: specsSet});
    await createAppElementWithPromiseValues(promiseValues);

    assertEquals(
        0,
        shoppingServiceApi.getCallCount('setUrlsForProductSpecificationsSet'));

    const table = appElement.$.summaryTable;
    const newUrl = 'https://example3.com/';
    table.dispatchEvent(new CustomEvent('url-change', {
      detail: {
        url: newUrl,
        index: 0,
      },
    }));

    assertEquals(
        1,
        shoppingServiceApi.getCallCount('setUrlsForProductSpecificationsSet'));
    assertArrayEquals(
        [{url: newUrl}, {url: 'https://example2.com/'}],
        shoppingServiceApi.getArgs('setUrlsForProductSpecificationsSet')[0][1]);
  });

  test('updates table on url removal', async () => {
    const testUrl = 'https://example.com/';
    const testId = 'foo';
    const promiseValues = createAppPromiseValues({
      urlsParam: [testUrl],
      specsSet: createSpecsSet({
        urls: [{url: testUrl}],
        uuid: {value: testId},
      }),
    });
    await createAppElementWithPromiseValues(promiseValues);

    const table = appElement.$.summaryTable;
    assertEquals(1, table.columns.length);
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertEquals(1, shoppingServiceApi.getCallCount('getProductInfoForUrl'));

    table.dispatchEvent(new CustomEvent('url-remove', {
      detail: {
        index: 0,
      },
    }));
    // Simulate an update from sync (as a result of the above change).
    callbackRouterRemote.onProductSpecificationsSetUpdated(
        createSpecsSet({urls: [], uuid: {value: testId}}));
    await waitAfterNextRender(appElement);

    assertEquals(0, table.columns.length);
    // Should not get called on an empty url list.
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertEquals(1, shoppingServiceApi.getCallCount('getProductInfoForUrl'));
  });

  test('deletes product specification set', async () => {
    const urlsParam = ['https://example.com/'];
    const promiseValues = createAppPromiseValues(
        {urlsParam: urlsParam, specsSet: createSpecsSet()});
    await createAppElementWithPromiseValues(promiseValues);

    const uuid =
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][2];
    const header =
        appElement.shadowRoot!.querySelector('product-specifications-header');
    assertTrue(!!header);
    header.dispatchEvent(new CustomEvent('delete-click'));

    assertEquals(
        1, shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
    assertEquals(
        uuid, shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[1]);
  });

  test('renames product specification set', async () => {
    const urlsParam = ['https://example.com/'];
    const promiseValues = createAppPromiseValues(
        {urlsParam: urlsParam, specsSet: createSpecsSet()});
    await createAppElementWithPromiseValues(promiseValues);

    const uuid =
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][2];
    const header =
        appElement.shadowRoot!.querySelector('product-specifications-header');
    assertTrue(!!header);
    const newName = 'new name';
    header.dispatchEvent(
        new CustomEvent('name-change', {detail: {name: newName}}));

    assertEquals(
        1,
        shoppingServiceApi.getCallCount('setNameForProductSpecificationsSet'));
    assertEquals(
        uuid,
        shoppingServiceApi.getArgs('setNameForProductSpecificationsSet')[1]);
    assertEquals(
        newName,
        shoppingServiceApi.getArgs('setNameForProductSpecificationsSet')[0][1]);
  });

  test('fire `url-order-update` event w/ id param', async () => {
    const specsSetUrls = [{url: 'https://0'}, {url: 'https://1'}];
    const testId = 'foo123';
    const specsSet =
        createSpecsSet({urls: specsSetUrls, uuid: {value: testId}});
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));
    const promiseValues = createAppPromiseValues({
      idParam: testId,
      specsSet: specsSet,
    });
    await createAppElementWithPromiseValues(promiseValues);

    const table = appElement.$.summaryTable;
    table.dispatchEvent(new Event('url-order-update'));

    const args = await shoppingServiceApi.whenCalled(
        'setUrlsForProductSpecificationsSet');
    assertEquals(2, args.length);
    assertArrayEquals(specsSetUrls, args[1]);
  });

  test('fire `url-order-update` event w/ url param', async () => {
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://0', 'https://1'],
    });
    await createAppElementWithPromiseValues(promiseValues);

    const table = appElement.$.summaryTable;
    table.dispatchEvent(new Event('url-order-update'));

    const args =
        await shoppingServiceApi.whenCalled('addProductSpecificationsSet');
    assertEquals(2, args.length);
    assertArrayEquals([{url: 'https://0'}, {url: 'https://1'}], args[1]);
  });

  suite('Header', () => {
    test('displays correct subtitle for retrieved sets', async () => {
      const specsSet = createSpecsSet({
        name: 'fooName',
      });
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsSetByUuid',
          Promise.resolve({set: specsSet}));

      const promiseValues = createAppPromiseValues({idParam: 'foo123'});
      await createAppElementWithPromiseValues(promiseValues);

      assertEquals('fooName', appElement.$.header.subtitle);
    });

    test('displays correct subtitle for created sets', async () => {
      const urlsParam = ['https://example3.com/', 'https://example4.com/'];
      const promiseValues = createAppPromiseValues({
        idParam: '',
        urlsParam: urlsParam,
      });
      await createAppElementWithPromiseValues(promiseValues);

      // TODO(b/338427523): Parameterize this test once there is UI for
      // choosing the name.
      assertEquals('Product specs', appElement.$.header.subtitle);
    });

    test('displays correct subtitle for empty state', async () => {
      router.setResultFor('getCurrentQuery', '');
      await createAppElement();

      assertEquals(null, appElement.$.header.subtitle);
    });
  });

  suite('EmptyState', () => {
    test('shows empty state if app loads without urls', () => {
      router.setResultFor('getCurrentQuery', '');
      createAppElement();

      assertNotStyle($$(appElement, '#empty')!, 'display', 'none');
      assertStyle($$(appElement, '#specs')!, 'display', 'none');
      const footer = appElement.shadowRoot!.querySelector('#footer');
      assertFalse(isVisible(footer));
    });

    test('hides empty state if app loads with urls', async () => {
      const urlsParam = ['https://example.com/', 'https://example2.com/'];
      const promiseValues = createAppPromiseValues({urlsParam: urlsParam});
      await createAppElementWithPromiseValues(promiseValues);

      assertStyle($$(appElement, '#empty')!, 'display', 'none');
      assertNotStyle($$(appElement, '#specs')!, 'display', 'none');
    });

    test('hides empty state after product selection', async () => {
      const url = 'https://example.com/';
      const productTabs = [{
        title: 'title',
        url: stringToMojoUrl(url),
      }];
      shoppingServiceApi.setResultFor(
          'getUrlInfosForProductTabs',
          Promise.resolve({urlInfos: productTabs}));
      shoppingServiceApi.setResultFor(
          'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));
      const promiseValues = createAppPromiseValues({
        urlsParam: [],
        infos: [createInfo({
          clusterId: BigInt(123),
          productUrl: {url: 'https://example.com/'},
        })],
      });
      await createAppElementWithPromiseValues(promiseValues);

      assertEquals(0, appElement.$.summaryTable.columns.length);

      // Open the product selection menu and select the first item.
      const productSelector = appElement.$.productSelector;
      productSelector.$.currentProductContainer.click();
      await waitAfterNextRender(appElement);
      const menu = productSelector.$.productSelectionMenu;
      const crActionMenu = menu.$.menu.get();
      assertTrue(crActionMenu.open);
      const dropdownItem =
          crActionMenu.querySelector<HTMLElement>('.dropdown-item')!;
      dropdownItem.click();
      await waitAfterNextRender(appElement);

      // The table should be updated with the selected URL.
      assertStyle($$(appElement, '#empty')!, 'display', 'none');
      assertNotStyle($$(appElement, '#specs')!, 'display', 'none');
      const tableColumns = appElement.$.summaryTable.columns;
      assertEquals(1, tableColumns.length);
      assertEquals(url, tableColumns[0]!.selectedItem.url);
    });

    test('removing last column shows empty state', async () => {
      const testUrl = 'https://example.com/';
      const testId = 'foo';
      const promiseValues = createAppPromiseValues({
        urlsParam: [testUrl],
        specsSet: createSpecsSet({
          urls: [{url: testUrl}],
          uuid: {value: testId},
        }),
      });
      await createAppElementWithPromiseValues(promiseValues);
      const table = appElement.$.summaryTable;
      assertEquals(1, table.columns.length);
      assertStyle($$(appElement, '#empty')!, 'display', 'none');
      assertNotStyle($$(appElement, '#specs')!, 'display', 'none');

      table.dispatchEvent(new CustomEvent('url-remove', {
        detail: {
          index: 0,
        },
      }));
      // Simulate an update from sync (as a result of the above change).
      callbackRouterRemote.onProductSpecificationsSetUpdated(
          createSpecsSet({urls: [], uuid: {value: testId}}));
      await waitAfterNextRender(appElement);

      assertEquals(0, table.columns.length);
      assertNotStyle($$(appElement, '#empty')!, 'display', 'none');
      assertStyle($$(appElement, '#specs')!, 'display', 'none');
    });
  });

  test('sends feedback', async () => {
    const urlsParam = ['https://example.com/'];
    const promiseValues = createAppPromiseValues(
        {urlsParam: urlsParam, specsSet: createSpecsSet()});
    await createAppElementWithPromiseValues(promiseValues);

    function updateCrFeedbackButtons(option: CrFeedbackOption) {
      appElement.$.feedbackButtons.dispatchEvent(
          new CustomEvent('selected-option-changed', {
            bubbles: true,
            composed: true,
            detail: {value: option},
          }));
    }

    updateCrFeedbackButtons(CrFeedbackOption.THUMBS_DOWN);
    let feedbackArgs = await shoppingServiceApi.whenCalled(
        'setProductSpecificationsUserFeedback');
    assertEquals(UserFeedback.kThumbsDown, feedbackArgs);
    shoppingServiceApi.resetResolver('setProductSpecificationsUserFeedback');

    updateCrFeedbackButtons(CrFeedbackOption.THUMBS_UP);
    feedbackArgs = await shoppingServiceApi.whenCalled(
        'setProductSpecificationsUserFeedback');
    assertEquals(UserFeedback.kThumbsUp, feedbackArgs);
    shoppingServiceApi.resetResolver('setProductSpecificationsUserFeedback');

    updateCrFeedbackButtons(CrFeedbackOption.UNSPECIFIED);
    feedbackArgs = await shoppingServiceApi.whenCalled(
        'setProductSpecificationsUserFeedback');
    assertEquals(UserFeedback.kUnspecified, feedbackArgs);
  });
});
