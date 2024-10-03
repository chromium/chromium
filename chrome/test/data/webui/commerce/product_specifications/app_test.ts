// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/app.js';

import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {COLUMN_MODIFICATION_HISTOGRAM_NAME, CompareTableColumnAction} from 'chrome://compare/app.js';
import type {ProductSpecificationsElement} from 'chrome://compare/app.js';
import type {ProductSelectorElement} from 'chrome://compare/product_selector.js';
import {Router} from 'chrome://compare/router.js';
import type {ProductInfo, ProductSpecifications, ProductSpecificationsProduct, ProductSpecificationsSet, ProductSpecificationsValue} from 'chrome://compare/shopping_service.mojom-webui.js';
import {WindowProxy} from 'chrome://compare/window_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {PageCallbackRouter, UserFeedback} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {$$, installMock} from './test_support.js';

function createProductInfo(overrides?: Partial<ProductInfo>): ProductInfo {
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
        summary: [],
        buyingOptionsUrl: {url: ''},
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
  productInfos: ProductInfo[];
  specsSet: ProductSpecificationsSet|null;
  urlToPageTitleFromHistoryMap: Map<string, string>;
  minLoadingAnimationMs: number;
}

function createAppPromiseValues(overrides?: Partial<AppPromiseValues>):
    AppPromiseValues {
  return Object.assign(
      {
        idParam: '',
        urlsParam: '',
        specs: createSpecs(),
        productInfos: [createProductInfo()],
        specsSet: null,
        urlToPageTitleFromHistoryMap: new Map<string, string>(),
        minLoadingAnimationMs: 0,
      },
      overrides);
}

suite('AppTest', () => {
  let appElement: ProductSpecificationsElement;
  let windowProxy: TestMock<WindowProxy>;
  const mockOpenWindowProxy = TestMock.fromClass(OpenWindowProxyImpl);

  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);
  const callbackRouter = new PageCallbackRouter();
  const callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
  const router = TestMock.fromClass(Router);

  async function createAppElement(): Promise<ProductSpecificationsElement> {
    appElement = document.createElement('product-specifications-app');
    document.body.appendChild(appElement);
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
          for (const info of promiseValues.productInfos) {
            if (info.productUrl.url === url.url) {
              return Promise.resolve({productInfo: info});
            }
          }
          const emptyInfo = createProductInfo();
          return Promise.resolve({productInfo: emptyInfo});
        });
    shoppingServiceApi.setResultMapperFor(
        'getPageTitleFromHistory', (url: Url) => {
          return Promise.resolve({
            title:
                promiseValues.urlToPageTitleFromHistoryMap.get(url.url) ?? '',
          });
        });

    const appElement = await createAppElement();
    appElement.resetMinLoadingAnimationMsForTesting(
        promiseValues.minLoadingAnimationMs);
    await flushTasks();

    return appElement;
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      defaultTableTitle: 'title',
      priceRowTitle: 'price',
      productSummaryRowTitle: 'summary',
    });
    shoppingServiceApi.reset();
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsFeatureState', Promise.resolve({
          state: {
            isSyncingTabCompare: true,
            canLoadFullPageUi: true,
            canManageSets: true,
            canFetchData: true,
            isAllowedForEnterprise: true,
            isQualityLoggingAllowed: true,
          },
        }));
    shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);
    shoppingServiceApi.setResultFor(
        'maybeShowProductSpecificationDisclosure',
        Promise.resolve({show: false}));
    BrowserProxyImpl.setInstance(shoppingServiceApi);
    router.reset();
    Router.setInstance(router);
    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('onLine', true);
    OpenWindowProxyImpl.setInstance(mockOpenWindowProxy);
  });

  test('calls shopping service when there are url params', async () => {
    const urlsParam = ['https://example.com/', 'https://example2.com/'];
    router.setResultFor(
        'getCurrentQuery',
        new URLSearchParams('urls=' + JSON.stringify(urlsParam)));
    createAppElement();
    await shoppingServiceApi.whenCalled('addProductSpecificationsSet');

    assertEquals(
        1,
        shoppingServiceApi.getCallCount(
            'getProductSpecificationsFeatureState'));
    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        1, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
    assertEquals(
        'title',
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][0]);
    assertArrayEquals(
        urlsParam.map(url => ({url})),
        shoppingServiceApi.getArgs('addProductSpecificationsSet')[0][1]);
  });

  test('handles invalid route', async () => {
    router.setResultFor(
        'getCurrentQuery', new URLSearchParams('urls=INVALID_JSON'));
    await createAppElement();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        0, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
  });

  test('handles missing router', async () => {
    router.setResultFor('getCurrentQuery', new URLSearchParams(''));
    await createAppElement();

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
        'title',
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
          url: {url: 'http://example.com/citation'},
          title: '',
          faviconUrl: {url: ''},
          thumbnailUrl: {url: ''},
        }],
      }],
      specificationDescriptions: [
        {
          label: 'label',
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
      summary: [{
        text: 'product summary',
        urls: [],
      }],
      buyingOptionsUrl: {url: 'https://example.com/jackpot/'},
    });
    const productInfo1 = createProductInfo({
      clusterId: BigInt(123),
      title: 'qux',
      productUrl: {url: 'https://example.com/'},
      imageUrl: {url: 'qux.com/image'},
      currentPrice: '$100',
    });
    const productInfo2 = createProductInfo({
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
      productInfos: [
        productInfo1,
        productInfo2,
        createProductInfo({clusterId: BigInt(0)}),
      ],
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
              imageUrl: productInfo1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', content: '$100'},
              {
                title: 'summary',
                content: {
                  attributes: [],
                  summary: [{
                    text: 'product summary',
                    urls: [],
                  }],
                },
              },
              {
                title: detailTitle,
                content: {
                  attributes: [{label: 'label', value: 'bar, baz'}],
                  summary: [{
                    text: 'summary',
                    urls: [{
                      url: {url: 'http://example.com/citation'},
                      title: '',
                      faviconUrl: {url: ''},
                      thumbnailUrl: {url: ''},
                    }],
                  }],
                },
              },
              {
                title: null,
                content: {jackpotUrl: specsProduct1.buyingOptionsUrl.url},
              },
            ],
          },
          {
            selectedItem: {
              // If the product spec doesn't have a title, the column should
              // use the title from the product info.
              title: productInfo2.title,
              url: 'https://example2.com/',
              imageUrl: productInfo2.imageUrl.url,
            },
            // Since this item's product dimension values have no ID, its
            // `productDetails` should have empty strings for `description` and
            // summary`. Its `jackpotUrl` should also be empty since no price
            // insights are available.
            productDetails: [
              {title: 'price', content: null},
              {title: 'summary', content: {attributes: [], summary: []}},
              {title: detailTitle, content: null},
              {title: null, content: {jackpotUrl: ''}},
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
    const productInfo1 = createProductInfo({
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
      productInfos: [productInfo1],
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
              imageUrl: productInfo1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', content: null},
              {title: 'summary', content: {attributes: [], summary: []}},
              {
                title: detailTitle,
                content: {
                  attributes: [{label: '', value: 'bar'}],
                  summary: [],
                },
              },
              {title: null, content: {jackpotUrl: ''}},
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
      buyingOptionsUrl: {url: 'https://example.com/jackpot1'},
    });
    const productInfo1 = createProductInfo({
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
      buyingOptionsUrl: {url: 'https://example.com/jackpot2'},
    });
    const productInfo2 = createProductInfo({
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
      productInfos: [productInfo1, productInfo2],
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
              imageUrl: productInfo1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', content: null},
              {title: 'summary', content: {attributes: [], summary: []}},
              {
                title: detailTitle,
                content: {
                  attributes: [{label: '', value: 'desc 1'}],
                  summary: [],
                },
              },
              {
                title: null,
                content: {jackpotUrl: specsProduct1.buyingOptionsUrl.url},
              },
            ],
          },
          {
            selectedItem: {
              title: specsProduct2.title,
              url: 'https://example.com/2',
              imageUrl: productInfo2.imageUrl.url,
            },
            productDetails: [
              {title: 'price', content: null},
              {title: 'summary', content: {attributes: [], summary: []}},
              {
                title: detailTitle,
                content: {
                  attributes: [{label: '', value: 'desc 2'}],
                  summary: [],
                },
              },
              {
                title: null,
                content: {jackpotUrl: specsProduct2.buyingOptionsUrl.url},
              },
            ],
          },
        ],
        tableColumns);
  });

  test(
      'uses history page title when available if other titles are unavailable',
      async () => {
        const specsProduct = createSpecsProduct({
          productClusterId: BigInt(123),
          title: 'foo',
          summary: [{
            text: 'product summary',
            urls: [],
          }],
        });
        const productInfo = createProductInfo({
          clusterId: BigInt(123),
          title: 'foo',
          productUrl: {url: 'https://example.com/'},
          imageUrl: {url: 'foo.com/image'},
        });
        const urlInHistory = 'https://example2.com/';
        const pageTitleInHistory = 'foo title';

        const promiseValues = createAppPromiseValues({
          urlsParam: [productInfo.productUrl.url, urlInHistory],
          specs: createSpecs({
            products: [specsProduct],
          }),
          productInfos: [
            productInfo,
            createProductInfo({clusterId: BigInt(0)}),
          ],
          urlToPageTitleFromHistoryMap: new Map<string, string>([
            [productInfo.productUrl.url, 'bar'],
            [urlInHistory, pageTitleInHistory],
          ]),
        });
        await createAppElementWithPromiseValues(promiseValues);

        const tableColumns = appElement.$.summaryTable.columns;
        assertEquals(2, tableColumns.length);
        assertEquals(specsProduct.title, tableColumns[0]!.selectedItem.title);
        assertEquals(pageTitleInHistory, tableColumns[1]!.selectedItem.title);
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
      buyingOptionsUrl: {url: 'https://example.com/jackpot1'},
    });
    const productInfo1 = createProductInfo({
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
      buyingOptionsUrl: {url: 'https://example.com/jackpot2'},
    });
    const productInfo2 = createProductInfo({
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
      productInfos: [productInfo1, productInfo2],
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
              imageUrl: productInfo2.imageUrl.url,
            },
            productDetails: [
              {title: 'price', content: null},
              {title: 'summary', content: {attributes: [], summary: []}},
              {
                title: rowTitle,
                content: {
                  attributes: [{label: '', value: 'desc 2'}],
                  summary: [],
                },
              },
              {
                title: null,
                content: {jackpotUrl: specsProduct2.buyingOptionsUrl.url},
              },
            ],
          },
          {
            selectedItem: {
              title: specsProduct1.title,
              url: 'https://example.com/1',
              imageUrl: productInfo1.imageUrl.url,
            },
            productDetails: [
              {title: 'price', content: null},
              {title: 'summary', content: {attributes: [], summary: []}},
              {
                title: rowTitle,
                content: {
                  attributes: [{label: '', value: 'desc 1'}],
                  summary: [],
                },
              },
              {
                title: null,
                content: {jackpotUrl: specsProduct1.buyingOptionsUrl.url},
              },
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
    const info = createProductInfo({
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
      productInfos: [info],
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
    const info = createProductInfo({
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
      productInfos: [info],
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

  test('creating new set triggers disclosure', async () => {
    const productTabs = [{
      title: 'title',
      url: stringToMojoUrl('https://example.com/'),
    }];
    shoppingServiceApi.setResultFor(
        'getUrlInfosForProductTabs', Promise.resolve({urlInfos: productTabs}));
    shoppingServiceApi.setResultFor(
        'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));
    // Mock that disclosure dialog should be shown.
    shoppingServiceApi.setResultFor(
        'maybeShowProductSpecificationDisclosure',
        Promise.resolve({disclosureShown: true}));
    createAppElement();

    // Click on the "add column" button and select the first (only) item.
    const newColSelector = appElement.$.newColumnSelector;
    newColSelector.$.button.click();
    await waitAfterNextRender(appElement);
    const menu = newColSelector.$.productSelectionMenu;
    const crActionMenu = menu.$.menu.get();
    assertTrue(crActionMenu.open);
    const dropdownItem =
        crActionMenu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!dropdownItem);
    dropdownItem.click();
    await waitAfterNextRender(appElement);

    await shoppingServiceApi.whenCalled(
        'maybeShowProductSpecificationDisclosure');
    const showArgs =
        shoppingServiceApi.getArgs('maybeShowProductSpecificationDisclosure');
    assertEquals('https://example.com/', showArgs[0][0][0].url);
    // Product spec set title will be empty by default.
    assertEquals('', showArgs[0][1]);
    assertEquals(
        0, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
  });

  test('populate table triggers disclosure', async () => {
    // Mock that disclosure dialog should be shown.
    shoppingServiceApi.setResultFor(
        'maybeShowProductSpecificationDisclosure',
        Promise.resolve({disclosureShown: true}));
    // Mock that we are opening the page with an existing set.
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
    const info = createProductInfo({
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
      productInfos: [info],
    });
    const specsSet = createSpecsSet(
        {urls: [{url: 'https://example.com/'}], uuid: {value: testId}});
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsSetByUuid', Promise.resolve({set: specsSet}));

    await createAppElementWithPromiseValues(promiseValues);
    await shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');

    assertTrue(isVisible(appElement.$.empty));
    assertFalse(isVisible(appElement.$.specs));
    assertEquals(
        1,
        shoppingServiceApi.getCallCount(
            'maybeShowProductSpecificationDisclosure'));
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
    const info = createProductInfo({
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
      productInfos: [info],
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
    await shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');
    // Check whether we should show disclosure when there is an existing set.
    assertEquals(
        1,
        shoppingServiceApi.getCallCount(
            'maybeShowProductSpecificationDisclosure'));

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
    // We should not try to show the disclosure when trying to add product to an
    // existing set.
    assertEquals(
        1,
        shoppingServiceApi.getCallCount(
            'maybeShowProductSpecificationDisclosure'));
  });

  suite('metrics', () => {
    let metrics: MetricsTracker;
    const tabInfos = [{
      title: 'title',
      url: stringToMojoUrl('https://example.com'),
    }];

    setup(async () => {
      metrics = fakeMetricsPrivate();
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
      const info = createProductInfo({
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
        productInfos: [info],
      });
      const specsSet = createSpecsSet(
          {urls: [{url: 'https://example.com/'}], uuid: {value: testId}});
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsSetByUuid',
          Promise.resolve({set: specsSet}));

      await createAppElementWithPromiseValues(promiseValues);
    });

    async function clickFirstAvailableItemInFirstColumn() {
      const table = appElement.$.summaryTable;
      const selector = table.shadowRoot!.querySelector<ProductSelectorElement>(
          'product-selector');
      assertTrue(!!selector);
      selector.$.currentProductContainer.click();
      await waitAfterNextRender(appElement);
      const crActionMenu = selector.$.productSelectionMenu.$.menu.get();
      assertTrue(crActionMenu.open);
      const item = crActionMenu.querySelector<HTMLElement>('.dropdown-item')!;
      item.click();
      await waitAfterNextRender(appElement);
    }

    async function clickFirstAvailableItemInNewColumnSelector() {
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
    }

    test('add column from suggested', async () => {
      shoppingServiceApi.setResultFor(
          'getUrlInfosForProductTabs', Promise.resolve({urlInfos: tabInfos}));
      shoppingServiceApi.setResultFor(
          'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));
      await shoppingServiceApi.whenCalled(
          'getProductSpecificationsFeatureState');

      await clickFirstAvailableItemInNewColumnSelector();

      assertEquals(
          1,
          metrics.count(
              COLUMN_MODIFICATION_HISTOGRAM_NAME,
              CompareTableColumnAction.ADD_FROM_SUGGESTED));
    });

    test('add column from recently viewed', async () => {
      shoppingServiceApi.setResultFor(
          'getUrlInfosForProductTabs', Promise.resolve({urlInfos: []}));
      shoppingServiceApi.setResultFor(
          'getUrlInfosForRecentlyViewedTabs',
          Promise.resolve({urlInfos: tabInfos}));
      await shoppingServiceApi.whenCalled(
          'getProductSpecificationsFeatureState');

      await clickFirstAvailableItemInNewColumnSelector();

      assertEquals(
          1,
          metrics.count(
              COLUMN_MODIFICATION_HISTOGRAM_NAME,
              CompareTableColumnAction.ADD_FROM_RECENTLY_VIEWED));
    });

    test('remove column', async () => {
      shoppingServiceApi.setResultFor(
          'getUrlInfosForProductTabs', Promise.resolve({urlInfos: []}));
      shoppingServiceApi.setResultFor(
          'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));

      await clickFirstAvailableItemInFirstColumn();

      assertEquals(
          1,
          metrics.count(
              COLUMN_MODIFICATION_HISTOGRAM_NAME,
              CompareTableColumnAction.REMOVE));
    });

    test('Update column from suggested', async () => {
      shoppingServiceApi.setResultFor(
          'getUrlInfosForProductTabs', Promise.resolve({urlInfos: tabInfos}));
      shoppingServiceApi.setResultFor(
          'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));

      await clickFirstAvailableItemInFirstColumn();
      assertEquals(
          1,
          metrics.count(
              COLUMN_MODIFICATION_HISTOGRAM_NAME,
              CompareTableColumnAction.UPDATE_FROM_SUGGESTED));
    });

    test('Update column from recently viewed', async () => {
      shoppingServiceApi.setResultFor(
          'getUrlInfosForProductTabs', Promise.resolve({urlInfos: []}));
      shoppingServiceApi.setResultFor(
          'getUrlInfosForRecentlyViewedTabs',
          Promise.resolve({urlInfos: tabInfos}));

      await clickFirstAvailableItemInFirstColumn();

      assertEquals(
          1,
          metrics.count(
              COLUMN_MODIFICATION_HISTOGRAM_NAME,
              CompareTableColumnAction.UPDATE_FROM_RECENTLY_VIEWED));
    });
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
    const info = createProductInfo({
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
      productInfos: [info],
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
    const minLoadingAnimationMs = 40;
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
      minLoadingAnimationMs: minLoadingAnimationMs,
    });
    // Needs to await in order to load elements.
    await createAppElementWithPromiseValues(promiseValues);
    await shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');

    assertTrue(isVisible(appElement.$.loading));
    assertFalse(isVisible(appElement.$.summaryTable));

    // Wait for the loading animation to finish.
    await new Promise(res => setTimeout(res, minLoadingAnimationMs));
    assertFalse(isVisible(appElement.$.loading));
  });

  test('disables menu button while loading', async () => {
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
      minLoadingAnimationMs: 500,
    });
    createAppElementWithPromiseValues(promiseValues);
    await flushTasks();

    assertTrue(appElement.$.header.$.menuButton.disabled);
  });

  test('show feedback loading state while loading', async () => {
    const minLoadingAnimationMs = 80;
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
      minLoadingAnimationMs: minLoadingAnimationMs,
    });
    // Needs to await in order to load elements.
    await createAppElementWithPromiseValues(promiseValues);
    await shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');

    const feedbackLoading =
        appElement.shadowRoot!.querySelector('#feedbackLoading');
    assertTrue(!!feedbackLoading);
    const feedbackButtons =
        appElement.shadowRoot!.querySelector('#feedbackButtons');
    assertTrue(!!feedbackButtons);

    assertTrue(isVisible(feedbackLoading));
    assertFalse(isVisible(feedbackButtons));

    // Wait for the loading animation to finish.
    await new Promise(res => setTimeout(res, minLoadingAnimationMs));
    assertFalse(isVisible(feedbackLoading));
    assertTrue(isVisible(feedbackButtons));
  });

  test('feedback hidden if not allowed', async () => {
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsFeatureState', Promise.resolve({
          state: {
            isSyncingTabCompare: true,
            canLoadFullPageUi: true,
            canManageSets: true,
            canFetchData: true,
            isAllowedForEnterprise: true,
            isQualityLoggingAllowed: false,
          },
        }));
    const minLoadingAnimationMs = 10;
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
      minLoadingAnimationMs: minLoadingAnimationMs,
    });
    createAppElementWithPromiseValues(promiseValues);
    await flushTasks();
    const feedbackLoading =
        appElement.shadowRoot!.querySelector('#feedbackLoading');
    const feedbackButtons =
        appElement.shadowRoot!.querySelector('#feedbackButtons');

    assertFalse(isVisible(feedbackLoading));
    assertFalse(isVisible(feedbackButtons));

    // Wait for the loading animation to finish.
    await new Promise(res => setTimeout(res, minLoadingAnimationMs));

    assertFalse(isVisible(feedbackLoading));
    assertFalse(isVisible(feedbackButtons));
  });

  test('shows learn more link', async () => {
    const testEmail = 'test@gmail.com';
    loadTimeData.overrideValues({userEmail: testEmail});
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
    });
    await createAppElementWithPromiseValues(promiseValues);
    const learnMoreLink =
        appElement.shadowRoot!.querySelector('#learnMoreLink');
    const disclaimer = appElement.shadowRoot!.querySelector('#disclaimer');

    assertTrue(!!learnMoreLink);
    assertTrue(isVisible(learnMoreLink));
    assertEquals(
        loadTimeData.getString('compareLearnMoreUrl'),
        learnMoreLink!.getAttribute('href'));

    assertTrue(!!disclaimer);
    assertTrue(!!disclaimer.textContent);
    // Remove the link part before verifying the string to avoid verifying the
    // spaces due to the templated string.
    const disclaimerText =
        disclaimer!.textContent!.replace(learnMoreLink!.textContent!, '')
            .trim();
    assertEquals(
        loadTimeData.getStringF('experimentalFeatureDisclaimer', testEmail),
        disclaimerText);
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
    appElement.$.header.dispatchEvent(new CustomEvent('delete-click'));

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
    const newName = 'new name';
    appElement.$.header.dispatchEvent(
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
      assertEquals('title', appElement.$.header.subtitle);
    });

    test('displays correct subtitle for empty state', async () => {
      router.setResultFor('getCurrentQuery', '');
      await createAppElement();

      assertEquals(null, appElement.$.header.subtitle);
    });
  });

  suite('EmptyState', () => {
    test('shows empty state if app loads without urls', async () => {
      router.setResultFor('getCurrentQuery', '');
      createAppElement();
      await flushTasks();

      assertTrue(isVisible(appElement.$.empty));
      assertFalse(isVisible(appElement.$.specs));
      const footer = appElement.shadowRoot!.querySelector('#footer');
      assertFalse(isVisible(footer));
    });

    test('hides empty state if app loads with urls', async () => {
      const urlsParam = ['https://example.com/', 'https://example2.com/'];
      const promiseValues = createAppPromiseValues({urlsParam: urlsParam});
      await createAppElementWithPromiseValues(promiseValues);

      assertFalse(isVisible(appElement.$.empty));
      assertTrue(isVisible(appElement.$.specs));
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
        productInfos: [createProductInfo({
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
      assertFalse(isVisible(appElement.$.empty));
      assertTrue(isVisible(appElement.$.specs));
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
      assertFalse(isVisible(appElement.$.empty));
      assertTrue(isVisible(appElement.$.specs));

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
      assertTrue(isVisible(appElement.$.empty));
      assertFalse(isVisible(appElement.$.specs));
    });
  });

  test('error toast shown with two URLs and no table', async () => {
    const productInfo1 = createProductInfo({
      clusterId: BigInt(123),
      title: 'Product 1',
      productUrl: {url: 'https://example.com/1'},
      imageUrl: {url: 'http://example.com/image1.png'},
    });

    const productInfo2 = createProductInfo({
      clusterId: BigInt(456),
      title: 'Product 2',
      productUrl: {url: 'https://example.com/2'},
      imageUrl: {url: 'http://example.com/image2.png'},
    });

    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/1', 'https://example.com/2'],
      specs: createSpecs({
        productDimensionMap: new Map<bigint, string>(),
      }),
      productInfos: [productInfo1, productInfo2],
    });
    await createAppElementWithPromiseValues(promiseValues);

    // Any comparison with a valid product should return at least one column. In
    // this case the error toast should be shown.
    assertTrue(appElement.$.errorToast.open);
  });

  test('error toast not shown with one URL', async () => {
    const promiseValues = createAppPromiseValues({
      urlsParam: ['https://example.com/'],
      specs: createSpecs({
        productDimensionMap: new Map<bigint, string>(),
      }),
    });
    await createAppElementWithPromiseValues(promiseValues);

    // If there's only one URL in the comparison, don't show the error.
    assertFalse(appElement.$.errorToast.open);
  });

  suite('Offline', () => {
    test(
        'shows error state and offline toast if app loads offline',
        async () => {
          router.setResultFor(
              'getCurrentQuery',
              new URLSearchParams(
                  'urls=' + JSON.stringify('https://example.com/')));
          windowProxy.setResultFor('onLine', false);
          await createAppElement();

          assertTrue(isVisible(appElement.$.error));
          assertTrue(appElement.$.offlineToast.open);
        });

    test(
        `shows offline toast instead of making api call when
                  #delete is clicked`,
        async () => {
          // Arrange.
          const promiseValues = createAppPromiseValues({
            urlsParam: ['https://example.com/'],
            specsSet: createSpecsSet(),
          });
          await createAppElementWithPromiseValues(promiseValues);
          windowProxy.setResultFor('onLine', false);
          assertFalse(appElement.$.offlineToast.open);

          // Act.
          const header = appElement.$.header;
          header.$.menuButton.click();
          const menu = header.$.menu.$.menu;
          const menuItemButton =
              menu.get().querySelector<HTMLElement>('#delete');
          assertTrue(!!menuItemButton);
          menuItemButton.click();
          await flushTasks();

          // Assert.
          assertTrue(appElement.$.offlineToast.open);
          assertEquals(
              0,
              shoppingServiceApi.getCallCount(
                  'deleteProductSpecificationsSet'));
        });

    test(
        `shows offline toast instead of making api call when rename attempted`,
        async () => {
          // Arrange.
          const promiseValues = createAppPromiseValues({
            urlsParam: ['https://example.com/'],
            specsSet: createSpecsSet(),
          });
          await createAppElementWithPromiseValues(promiseValues);
          windowProxy.setResultFor('onLine', false);
          assertFalse(appElement.$.offlineToast.open);

          // Act.
          const header = appElement.$.header;
          header.$.menu.dispatchEvent(new CustomEvent('rename-click'));
          await waitAfterNextRender(header);
          const input = $$<CrInputElement>(header, '#input');
          assertTrue(!!input);
          input.value = 'foo';
          input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
          await flushTasks();

          // Assert.
          assertTrue(appElement.$.offlineToast.open);
          assertEquals(
              0,
              shoppingServiceApi.getCallCount(
                  'setNameForProductSpecificationsSet'));
        });

    test('hides offline toast if app comes back online', () => {
      windowProxy.setResultFor('onLine', false);
      createAppElement();
      assertTrue(appElement.$.offlineToast.open);

      window.dispatchEvent(new Event('online'));

      assertFalse(appElement.$.offlineToast.open);
    });

    test('hides offline toast if element is clicked', () => {
      windowProxy.setResultFor('onLine', false);
      createAppElement();
      assertTrue(appElement.$.offlineToast.open);

      appElement.click();

      assertFalse(appElement.$.offlineToast.open);
    });

    test('shows offline toast post-click if it is re-triggered', async () => {
      // Arrange.
      const promiseValues = createAppPromiseValues(
          {urlsParam: ['https://example.com/'], specsSet: createSpecsSet()});
      await createAppElementWithPromiseValues(promiseValues);
      windowProxy.setResultFor('onLine', false);
      assertFalse(appElement.$.offlineToast.open);

      // Act.
      const openTabButton =
          $$<HTMLElement>(appElement.$.summaryTable, '.open-tab-button');
      assertTrue(!!openTabButton);
      openTabButton.click();
      await waitAfterNextRender(appElement);

      // Assert.
      assertTrue(appElement.$.offlineToast.open);
      assertEquals(0, shoppingServiceApi.getCallCount('switchToOrOpenTab'));

      // Act.
      openTabButton.click();
      await flushTasks();

      // Assert.
      assertTrue(appElement.$.offlineToast.open);
      assertEquals(0, shoppingServiceApi.getCallCount('switchToOrOpenTab'));
    });
  });

  test('sends feedback', async () => {
    const urlsParam = ['https://example.com/'];
    const promiseValues = createAppPromiseValues(
        {urlsParam: urlsParam, specsSet: createSpecsSet()});
    await createAppElementWithPromiseValues(promiseValues);

    function updateCrFeedbackButtons(option: CrFeedbackOption) {
      const feedbackButtons =
          appElement.shadowRoot!.querySelector('#feedbackButtons');
      assertTrue(!!feedbackButtons);
      feedbackButtons!.dispatchEvent(
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

  suite('FeatureState', () => {
    test('feedback hidden if not allowed', async () => {
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsFeatureState', Promise.resolve({
            state: {
              isSyncingTabCompare: true,
              canLoadFullPageUi: true,
              canManageSets: true,
              canFetchData: true,
              isAllowedForEnterprise: true,
              isQualityLoggingAllowed: false,
            },
          }));
      const minLoadingAnimationMs = 10;
      const promiseValues = createAppPromiseValues({
        urlsParam: ['https://example.com/'],
        minLoadingAnimationMs: minLoadingAnimationMs,
      });
      createAppElementWithPromiseValues(promiseValues);
      await flushTasks();
      const feedbackLoading =
          appElement.shadowRoot!.querySelector('#feedbackLoading');
      const feedbackButtons =
          appElement.shadowRoot!.querySelector('#feedbackButtons');

      assertFalse(isVisible(feedbackLoading));
      assertFalse(isVisible(feedbackButtons));

      // Wait for the loading animation to finish.
      await new Promise(res => setTimeout(res, minLoadingAnimationMs));

      assertFalse(isVisible(feedbackLoading));
      assertFalse(isVisible(feedbackButtons));
    });

    test('shows sync state if user is not syncing', async () => {
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsFeatureState', Promise.resolve({
            state: {
              isSyncingTabCompare: false,
              canLoadFullPageUi: true,
              canManageSets: true,
              canFetchData: true,
              isAllowedForEnterprise: true,
            },
          }));
      await createAppElement();
      await shoppingServiceApi.whenCalled(
          'getProductSpecificationsFeatureState');

      assertTrue(isVisible(appElement.$.syncPromo));
      assertFalse(isVisible(appElement.$.error));
      assertFalse(isVisible(appElement.$.empty));
      assertFalse(isVisible(appElement.$.specs));
    });

    test('shows error state if disabled', async () => {
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsFeatureState', Promise.resolve({
            state: {
              isSyncingTabCompare: true,
              canLoadFullPageUi: true,
              canManageSets: true,
              canFetchData: false,
              isAllowedForEnterprise: true,
            },
          }));
      createAppElement();
      await shoppingServiceApi.whenCalled(
          'getProductSpecificationsFeatureState');

      assertTrue(isVisible(appElement.$.error));
      assertFalse(isVisible(appElement.$.syncPromo));
      assertFalse(isVisible(appElement.$.empty));
      assertFalse(isVisible(appElement.$.specs));
    });

    test('reload with sync screen', async () => {
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsFeatureState', Promise.resolve({
            state: {
              isSyncingTabCompare: true,
              canLoadFullPageUi: true,
              canManageSets: true,
              canFetchData: false,
              isAllowedForEnterprise: true,
            },
          }));
      await createAppElement();
      await shoppingServiceApi.whenCalled(
          'getProductSpecificationsFeatureState');

      assertTrue(isVisible(appElement.$.error));
      assertFalse(isVisible(appElement.$.syncPromo));
      assertFalse(isVisible(appElement.$.empty));
      assertFalse(isVisible(appElement.$.specs));

      shoppingServiceApi.reset();
      shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsFeatureState', Promise.resolve({
            state: {
              isSyncingTabCompare: false,
              canLoadFullPageUi: true,
              canManageSets: true,
              canFetchData: true,
              isAllowedForEnterprise: true,
            },
          }));

      window.dispatchEvent(new Event('focus'));
      await flushTasks();

      assertFalse(isVisible(appElement.$.error));
      assertTrue(isVisible(appElement.$.syncPromo));
      assertFalse(isVisible(appElement.$.empty));
      assertFalse(isVisible(appElement.$.specs));
    });

    test('sync button click when user not signed in', async () => {
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsFeatureState', Promise.resolve({
            state: {
              isSyncingTabCompare: false,
              canLoadFullPageUi: true,
              canManageSets: true,
              canFetchData: true,
              isAllowedForEnterprise: true,
              isSignedIn: false,
            },
          }));
      const appElement = await createAppElement();
      await flushTasks();
      shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');
      assertTrue(isVisible(appElement.$.syncPromo));

      appElement.$.turnOnSyncButton.click();
      shoppingServiceApi.whenCalled('showSyncSetupFlow');
    });

    test('sync button click when user is signed in', async () => {
      shoppingServiceApi.setResultFor(
          'getProductSpecificationsFeatureState', Promise.resolve({
            state: {
              isSyncingTabCompare: false,
              canLoadFullPageUi: true,
              canManageSets: true,
              canFetchData: true,
              isAllowedForEnterprise: true,
              isSignedIn: true,
            },
          }));
      const appElement = await createAppElement();
      await flushTasks();
      shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');
      assertTrue(isVisible(appElement.$.syncPromo));

      appElement.$.turnOnSyncButton.click();
      await flushTasks();
      assertEquals(0, shoppingServiceApi.getCallCount('showSyncSetupFlow'));

      const arg = await mockOpenWindowProxy.whenCalled('openUrl');
      assertEquals('chrome://settings/syncSetup/advanced', arg);
    });
  });
});
