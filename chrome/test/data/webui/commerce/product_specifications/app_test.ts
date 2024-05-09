// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/app.js';

import type {ProductSpecificationsElement} from 'chrome://compare/app.js';
import {Router} from 'chrome://compare/router.js';
import type {ProductInfo, ProductSpecifications, ProductSpecificationsProduct, ProductSpecificationsSet} from 'chrome://compare/shopping_service.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {PageCallbackRouter} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
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
    createAppElementWithPromiseValues(promiseValues);

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
    createAppElementWithPromiseValues(promiseValues);
    await flushTasks();

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
    createAppElementWithPromiseValues(promiseValues);
    await flushTasks();

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
      urlsParam: ['https://example.com/', 'https://example2.com/'],
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

  test('updates on selection change', async () => {
    const urlsParam = ['https://example.com/', 'https://example2.com/'];
    const promiseValues = createAppPromiseValues({urlsParam: urlsParam});
    await createAppElementWithPromiseValues(promiseValues);

    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertArrayEquals(
        urlsParam.map(url => ({url})),
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[0]);

    const table =
        appElement.shadowRoot!.querySelector('product-specifications-table');
    assertTrue(!!table);
    const newUrl = 'https://example3.com';
    table.dispatchEvent(new CustomEvent('url-change', {
      detail: {
        url: newUrl,
        index: 0,
      },
    }));

    assertEquals(
        2, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
    assertArrayEquals(
        [{url: newUrl}],
        shoppingServiceApi.getArgs('getProductSpecificationsForUrls')[1]);
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
      createAppElementWithPromiseValues(promiseValues);
      await flushTasks();

      assertEquals('fooName', appElement.$.header.subtitle);
    });

    test('displays correct subtitle for created sets', async () => {
      const urlsParam = ['https://example3.com/', 'https://example4.com/'];
      const promiseValues = createAppPromiseValues({
        idParam: '',
        urlsParam: urlsParam,
      });
      createAppElementWithPromiseValues(promiseValues);
      await flushTasks();

      // TODO(b/338427523): Parameterize this test once there is UI for choosing
      // the name.
      assertEquals('Product specs', appElement.$.header.subtitle);
    });

    test('displays correct subtitle for empty state', async () => {
      router.setResultFor('getCurrentQuery', '');
      createAppElement();
      await flushTasks();

      assertEquals(null, appElement.$.header.subtitle);
    });
  });

  suite('EmptyState', () => {
    test('shows empty state if app loads without urls', () => {
      router.setResultFor('getCurrentQuery', '');
      createAppElement();

      assertNotStyle($$(appElement, '#empty')!, 'display', 'none');
      assertStyle($$(appElement, '#specs')!, 'display', 'none');
    });

    test('hides empty state if app loads with urls', () => {
      const urlsParam = ['https://example.com/', 'https://example2.com/'];
      const promiseValues = createAppPromiseValues({urlsParam: urlsParam});
      createAppElementWithPromiseValues(promiseValues);

      assertStyle($$(appElement, '#empty')!, 'display', 'none');
      assertNotStyle($$(appElement, '#specs')!, 'display', 'none');
    });

    test('hides empty state after product selection', async () => {
      const url = 'https://example.com';
      const openTabs = [{
        title: 'title',
        url: stringToMojoUrl(url),
      }];
      shoppingServiceApi.setResultFor(
          'getUrlInfosForOpenTabs', Promise.resolve({urlInfos: openTabs}));
      shoppingServiceApi.setResultFor(
          'getUrlInfosForRecentlyViewedTabs', Promise.resolve({urlInfos: []}));
      const promiseValues = createAppPromiseValues({
        urlsParam: [],
        infos: [createInfo({clusterId: BigInt(123)})],
      });
      createAppElementWithPromiseValues(promiseValues);

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
      assertEquals(1, appElement.$.summaryTable.columns.length);
      assertEquals(url, appElement.$.summaryTable.columns[0]!.selectedItem.url);
    });
  });
});
