// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/commerce/shopping_list.js';
import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {ActionSource} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {ShoppingListElement} from 'chrome://bookmarks-side-panel.top-chrome/commerce/shopping_list.js';
import {ACTION_BUTTON_TRACK_IMAGE, ACTION_BUTTON_UNTRACK_IMAGE, LOCAL_STORAGE_EXPAND_STATUS_KEY} from 'chrome://bookmarks-side-panel.top-chrome/commerce/shopping_list.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {BookmarkProductInfo} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from '../test_bookmarks_api_proxy.js';

import {TestBrowserProxy} from './test_shopping_service_api_proxy.js';

suite('SidePanelShoppingListTest', () => {
  let shoppingList: ShoppingListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let shoppingServiceApi: TestBrowserProxy;
  let metrics: MetricsTracker;

  const products: BookmarkProductInfo[] = [
    {
      bookmarkId: BigInt(3),
      info: {
        title: 'Product Foo',
        clusterTitle: 'Product Cluster Foo',
        domain: 'foo.com',
        imageUrl: {url: 'chrome://resources/images/error.svg'},
        productUrl: {url: 'https://foo.com/product'},
        currentPrice: '$12',
        previousPrice: '$34',
        clusterId: BigInt(12345),
        categoryLabels: [],
      },
    },
    {
      bookmarkId: BigInt(4),
      info: {
        title: 'Product bar',
        clusterTitle: 'Product Cluster bar',
        domain: 'bar.com',
        imageUrl: {url: ''},
        productUrl: {url: 'https://foo.com/product'},
        currentPrice: '$15',
        previousPrice: '',
        clusterId: BigInt(12345),
        categoryLabels: [],
      },
    },
  ];

  function getProductElements(shoppingList: HTMLElement): HTMLElement[] {
    return Array.from(
        shoppingList.shadowRoot!.querySelectorAll('.product-item'));
  }

  function checkProductElementRender(
      element: HTMLElement, product: BookmarkProductInfo): void {
    assertEquals(
        product.info.title,
        element.querySelector('.product-title')!.textContent);
    assertEquals(
        product.info.domain,
        element.querySelector('.product-domain')!.textContent);

    const imageElement =
        element.querySelector<HTMLElement>('.product-image-container');
    const faviconElement = element.querySelector<HTMLElement>('.favicon-image');
    if (!product.info.imageUrl.url) {
      assertFalse(isVisible(imageElement));
      assertTrue(isVisible(faviconElement));
    } else {
      assertFalse(isVisible(faviconElement));
      assertTrue(isVisible(imageElement));
      const productImage =
          imageElement!.querySelector<HTMLElement>(
                           '.product-image')!.getAttribute('auto-src');
      assertEquals(productImage, product.info.imageUrl.url);
    }
    const priceElements = Array.from(element.querySelectorAll('.price'));
    if (!product.info.previousPrice) {
      assertEquals(1, priceElements.length);
      assertEquals(priceElements[0]!.textContent, product.info.currentPrice);
    } else {
      assertEquals(2, priceElements.length);
      assertEquals(product.info.currentPrice, priceElements[0]!.textContent);
      assertEquals(product.info.previousPrice, priceElements[1]!.textContent);
    }
    const actionButton = element.querySelector<HTMLElement>('.action-button');
    assertTrue(!!actionButton);
    assertEquals(
        ACTION_BUTTON_UNTRACK_IMAGE, actionButton.getAttribute('iron-icon'));
    assertEquals(
        actionButton.getAttribute('title'),
        loadTimeData.getString('shoppingListUntrackPriceButtonDescription'));
  }

  function checkActionButtonStatus(
      actionButton: HTMLElement, isTracking: boolean): void {
    if (isTracking) {
      assertEquals(
          ACTION_BUTTON_UNTRACK_IMAGE, actionButton.getAttribute('iron-icon'));
      assertEquals(
          loadTimeData.getString('shoppingListUntrackPriceButtonDescription'),
          actionButton.getAttribute('title'));
    } else {
      assertEquals(
          ACTION_BUTTON_TRACK_IMAGE, actionButton.getAttribute('iron-icon'));
      assertEquals(
          loadTimeData.getString('shoppingListTrackPriceButtonDescription'),
          actionButton.getAttribute('title'));
    }
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    metrics = fakeMetricsPrivate();

    bookmarksApi = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingServiceApi = new TestBrowserProxy();
    BrowserProxyImpl.setInstance(shoppingServiceApi);

    shoppingList = document.createElement('shopping-list');
    shoppingList.productInfos = products.slice();
    document.body.appendChild(shoppingList);

    await flushTasks();
  });

  teardown(() => {
    window.localStorage[LOCAL_STORAGE_EXPAND_STATUS_KEY] = undefined;
  });

  test('RenderShoppingList', async () => {
    const productElements = getProductElements(shoppingList);
    assertEquals(2, products.length);

    for (let i = 0; i < products.length; i++) {
      checkProductElementRender(productElements[i]!, products[i]!);
    }
  });

  test('OpensAndClosesShoppingList', async () => {
    let productElements = getProductElements(shoppingList);
    const arrowIcon =
        shoppingList.shadowRoot!.querySelector<HTMLElement>('#arrowIcon')!;
    assertTrue(arrowIcon.hasAttribute('open'));
    for (let i = 0; i < productElements.length; i++) {
      assertTrue(isVisible(productElements[i]!));
    }

    shoppingList.shadowRoot!.querySelector<HTMLElement>('.row')!.click();
    await flushTasks();
    assertFalse(arrowIcon.hasAttribute('open'));
    for (let i = 0; i < productElements.length; i++) {
      assertFalse(isVisible(productElements[i]!));
    }
    assertFalse(
        JSON.parse(window.localStorage[LOCAL_STORAGE_EXPAND_STATUS_KEY]));
    assertEquals(
        0,
        metrics.count(
            'Commerce.PriceTracking.SidePanel.TrackedProductsExpanded'));
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceTracking.SidePanel.TrackedProductsCollapsed'));

    shoppingList.shadowRoot!.querySelector<HTMLElement>('.row')!.click();
    await flushTasks();
    assertTrue(arrowIcon.hasAttribute('open'));
    productElements = getProductElements(shoppingList);
    for (let i = 0; i < productElements.length; i++) {
      assertTrue(isVisible(productElements[i]!));
    }
    assertTrue(
        JSON.parse(window.localStorage[LOCAL_STORAGE_EXPAND_STATUS_KEY]));
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceTracking.SidePanel.TrackedProductsExpanded'));
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceTracking.SidePanel.TrackedProductsCollapsed'));
  });

  test('OpensProductItem', async () => {
    getProductElements(shoppingList)[0]!.click();
    const [id, parentFolderDepth, , source] =
        await bookmarksApi.whenCalled('openBookmark');
    assertEquals(products[0]!.bookmarkId.toString(), id);
    assertEquals(0, parentFolderDepth);
    assertEquals(ActionSource.kPriceTracking, source);
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceTracking.SidePanel.ClickedTrackedProduct'));
  });

  test('OpensProductItemContextMenu', async () => {
    getProductElements(shoppingList)[0]!.dispatchEvent(
        new MouseEvent('contextmenu'));
    const [id, , , source] = await bookmarksApi.whenCalled('showContextMenu');
    assertEquals(products[0]!.bookmarkId.toString(), id);
    assertEquals(ActionSource.kPriceTracking, source);
  });

  test('OpensProductItemWithAuxClick', async () => {
    // Middle mouse button click.
    getProductElements(shoppingList)[0]!.dispatchEvent(
        new MouseEvent('auxclick', {button: 1}));
    const [id, parentFolderDepth, , source] =
        await bookmarksApi.whenCalled('openBookmark');
    assertEquals(products[0]!.bookmarkId.toString(), id);
    assertEquals(0, parentFolderDepth);
    assertEquals(ActionSource.kPriceTracking, source);
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceTracking.SidePanel.ClickedTrackedProduct'));

    bookmarksApi.resetResolver('openBookmark');

    // Non-middle mouse aux clicks.
    getProductElements(shoppingList)[0]!.dispatchEvent(
        new MouseEvent('auxclick', {button: 2}));
    assertEquals(0, bookmarksApi.getCallCount('openBookmark'));
    assertEquals(
        1,
        metrics.count(
            'Commerce.PriceTracking.SidePanel.ClickedTrackedProduct'));
  });

  test('InitializesShoppingListExpandStatus', async () => {
    window.localStorage[LOCAL_STORAGE_EXPAND_STATUS_KEY] = false;

    const shoppingListClosed = document.createElement('shopping-list');
    shoppingListClosed.productInfos = products;
    document.body.appendChild(shoppingListClosed);
    await flushTasks();

    const productElements = getProductElements(shoppingListClosed);
    assertEquals(2, products.length);
    for (let i = 0; i < products.length; i++) {
      assertFalse(isVisible(productElements[i]!));
    }
    assertFalse(
        shoppingListClosed.shadowRoot!.getElementById(
                                          'arrowIcon')!.hasAttribute('open'));
  });

  test('TracksAndUntracksPrice', async () => {
    const actionButton =
        getProductElements(shoppingList)[0]!.querySelector<HTMLElement>(
            '.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    let id = await shoppingServiceApi.whenCalled('untrackPriceForBookmark');
    assertEquals(id, products[0]!.bookmarkId);
    checkActionButtonStatus(actionButton, false);
    assertEquals(
        0, metrics.count('Commerce.PriceTracking.SidePanel.Track.BellButton'));
    assertEquals(
        1,
        metrics.count('Commerce.PriceTracking.SidePanel.Untrack.BellButton'));

    actionButton.click();
    id = await shoppingServiceApi.whenCalled('trackPriceForBookmark');
    assertEquals(id, products[0]!.bookmarkId);
    checkActionButtonStatus(actionButton, true);
    assertEquals(
        1, metrics.count('Commerce.PriceTracking.SidePanel.Track.BellButton'));
    assertEquals(
        1,
        metrics.count('Commerce.PriceTracking.SidePanel.Untrack.BellButton'));
  });

  test('ObservesTrackAndUntrackPriceForNewProduct', async () => {
    const newProduct = {
      bookmarkId: BigInt(5),
      info: {
        title: 'Product Baz',
        clusterTitle: 'Product Cluster Baz',
        domain: 'baz.com',
        imageUrl: {url: 'https://baz.com/image'},
        productUrl: {url: 'https://baz.com/product'},
        currentPrice: '$56',
        previousPrice: '$78',
        clusterId: BigInt(12345),
        categoryLabels: [],
      },
    };

    shoppingServiceApi.getCallbackRouterRemote().priceTrackedForBookmark(
        newProduct);
    await flushTasks();
    const productElements = getProductElements(shoppingList);
    assertEquals(3, productElements.length);
    checkProductElementRender(productElements[2]!, newProduct);

    const actionButtons =
        Array.from(shoppingList.shadowRoot!.querySelectorAll<HTMLElement>(
            '.action-button'));
    assertEquals(3, actionButtons.length);
    for (let i = 0; i < 3; i++) {
      checkActionButtonStatus(actionButtons[i]!, true);
    }

    shoppingServiceApi.getCallbackRouterRemote().priceUntrackedForBookmark(
        newProduct);
    await flushTasks();
    checkActionButtonStatus(actionButtons[0]!, true);
    checkActionButtonStatus(actionButtons[1]!, true);
    checkActionButtonStatus(actionButtons[2]!, false);
    assertEquals(3, getProductElements(shoppingList).length);
  });

  test('ObservesTrackAndUntrackPriceForExitingProduct', async () => {
    // Manually untrack price for bookmark with ID 3.
    const product = products[0]!;
    const actionButtonA =
        getProductElements(shoppingList)[0]!.querySelector<HTMLElement>(
            '.action-button');
    assertTrue(!!actionButtonA);
    actionButtonA.click();
    const id = await shoppingServiceApi.whenCalled('untrackPriceForBookmark');
    assertEquals(id, products[0]!.bookmarkId);
    checkActionButtonStatus(actionButtonA, false);

    shoppingServiceApi.getCallbackRouterRemote().priceTrackedForBookmark(product);
    await flushTasks();
    checkActionButtonStatus(actionButtonA, true);

    shoppingServiceApi.getCallbackRouterRemote().priceUntrackedForBookmark(
        product);
    await flushTasks();
    checkActionButtonStatus(actionButtonA, false);

    shoppingServiceApi.getCallbackRouterRemote().priceTrackedForBookmark(product);
    await flushTasks();
    checkActionButtonStatus(actionButtonA, true);
  });

  test('ObservesTrackedProductInfoUpdate', async () => {
    let productElements = getProductElements(shoppingList);
    assertEquals(2, products.length);

    for (let i = 0; i < products.length; i++) {
      checkProductElementRender(productElements[i]!, products[i]!);
    }

    const updatedProduct = {
      bookmarkId: BigInt(3),
      info: {
        title: 'Product Baz',
        clusterTitle: 'Product Cluster Baz',
        domain: 'baz.com',
        imageUrl: {url: 'chrome://resources/images/error.svg'},
        productUrl: {url: 'https://baz.com/product'},
        currentPrice: '$56',
        previousPrice: '$78',
        clusterId: BigInt(12345),
        categoryLabels: [],
      },
    };
    shoppingServiceApi.getCallbackRouterRemote().priceTrackedForBookmark(
        updatedProduct);
    await flushTasks();

    productElements = getProductElements(shoppingList);
    assertEquals(2, products.length);

    checkProductElementRender(productElements[0]!, updatedProduct);
    checkProductElementRender(productElements[1]!, products[1]!);
  });

  test('UntrackedItemsResetsWithProductInfos', async () => {
    let actionButton =
        getProductElements(shoppingList)[0]!.querySelector('cr-icon-button');
    assertTrue(!!actionButton);
    actionButton.click();
    const id = await shoppingServiceApi.whenCalled('untrackPriceForBookmark');
    assertEquals(id, products[0]!.bookmarkId);
    checkActionButtonStatus(actionButton, false);

    // Reset shoppingList.productInfos to empty and then re-initialize it, the
    // untracked items list should be reset to empty.
    shoppingList.productInfos = [];
    shoppingList.productInfos = products.slice();
    await microtasksFinished();

    actionButton =
        getProductElements(shoppingList)[0]!.querySelector('cr-icon-button');
    assertTrue(!!actionButton);
    checkActionButtonStatus(actionButton, true);
  });

  test('ShowErrorToastWhenTrackAndUntrackFailed', async () => {
    shoppingServiceApi.getCallbackRouterRemote().operationFailedForBookmark(
        products[0]!, true);
    await flushTasks();

    assertTrue(shoppingList.$.errorToast.open);
    shoppingList.$.errorToast.querySelector('cr-button')!.click();
    let id = await shoppingServiceApi.whenCalled('trackPriceForBookmark');
    assertEquals(id, products[0]!.bookmarkId);
    assertFalse(shoppingList.$.errorToast.open);

    shoppingServiceApi.getCallbackRouterRemote().operationFailedForBookmark(
        products[1]!, false);
    await flushTasks();

    assertTrue(shoppingList.$.errorToast.open);
    shoppingList.$.errorToast.querySelector('cr-button')!.click();
    id = await shoppingServiceApi.whenCalled('untrackPriceForBookmark');
    assertEquals(id, products[1]!.bookmarkId);
    assertFalse(shoppingList.$.errorToast.open);
  });
});
