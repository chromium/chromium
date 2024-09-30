// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {ensureLazyLoaded, ShoppingBrowserProxyImpl} from 'chrome://history/history.js';
import type {CrButtonElement, CrCheckboxElement, ProductSpecificationsListsElement} from 'chrome://history/history.js';
import {ShoppingPageCallbackRouter} from 'chrome://history/history.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';


suite('ProductSpecificationsListTest', () => {
  const shoppingServiceApi = TestMock.fromClass(ShoppingBrowserProxyImpl);
  let productSpecificationsList: ProductSpecificationsListsElement;

  const callbackRouter = new ShoppingPageCallbackRouter();
  const callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

  function createProductSpecsList(): ProductSpecificationsListsElement {
    productSpecificationsList =
        document.createElement('product-specifications-lists');
    document.body.appendChild(productSpecificationsList);
    return productSpecificationsList;
  }

  function initProductSets() {
    shoppingServiceApi.setResultFor(
        'getAllProductSpecificationsSets', Promise.resolve({
          sets: [
            {
              name: 'example1',
              uuid: {value: 'ex1'},
              urls: [{url: 'dot com 1'}],
            },
            {
              name: 'example2',
              uuid: {value: 'ex2'},
              urls: [{url: 'dot com 2a'}, {url: 'dot com 2b'}],
            },
            {
              name: 'example3',
              uuid: {value: 'ex3'},
              urls: [{url: 'dot com 3a'}, {url: 'dot com 3b'}],
            },
            {
              name: 'example4',
              uuid: {value: 'ex4'},
              urls: [{url: 'dot com 4a'}, {url: 'dot com 4b'}],
            },
          ],
        }));
  }

  function initProductSpecsState() {
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsFeatureState', Promise.resolve({
          state: {
            isSyncingTabCompare: true,
            canLoadFullPageUi: true,
            canManageSets: true,
            canFetchData: true,
            isAllowedForEnterprise: true,
          },
        }));
  }

  setup(function() {
    shoppingServiceApi.reset();
    shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);
    ShoppingBrowserProxyImpl.setInstance(shoppingServiceApi);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    initProductSets();
    initProductSpecsState();
    createProductSpecsList();
    return Promise.all([flushTasks]).then(() => {
      shoppingServiceApi.whenCalled('getAllProductSpecificationsSets');
      shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');
    });
  });

  test('load', async () => {
    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(4, items.length);
    const firstItem = items[0]!.item;
    assertEquals('example1', firstItem.name);
    assertEquals('ex1', firstItem.uuid.value);
    assertDeepEquals([{url: 'dot com 1'}], firstItem.urls);

    const secondItem = items[1]!.item;
    assertEquals('example2', secondItem.name);
    assertEquals('ex2', secondItem.uuid.value);
    assertDeepEquals(
        [{url: 'dot com 2a'}, {url: 'dot com 2b'}], secondItem.urls);

    assertDeepEquals(
        {
          name: 'example3',
          uuid: {value: 'ex3'},
          urls: [{url: 'dot com 3a'}, {url: 'dot com 3b'}],
        },
        items[2]!.item);
    assertDeepEquals(
        {
          name: 'example4',
          uuid: {value: 'ex4'},
          urls: [{url: 'dot com 4a'}, {url: 'dot com 4b'}],
        },
        items[3]!.item);
  });

  test('displays correct header', async () => {
    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(4, items.length);

    const cardTitleHeader = productSpecificationsList.shadowRoot!.querySelector(
        '#card-title-header');
    assertTrue(!!cardTitleHeader);
    const heading = cardTitleHeader!.textContent;
    assertTrue(!!heading);
    assertEquals('Comparison tables', heading.trim());
  });


  test(
      'selecting product set adds uuid to selected items list',
      async function() {
        const items = productSpecificationsList.shadowRoot!.querySelectorAll(
            'product-specifications-item');
        assertDeepEquals(new Set(), productSpecificationsList.selectedItems);

        const secondItem = items[1]!;
        const checkbox = secondItem.$.checkbox as CrCheckboxElement;
        checkbox.click();

        await checkbox.updateComplete;
        assertDeepEquals(
            new Set(['ex2']), productSpecificationsList.selectedItems);
      });

  test('consuming menu event opens menu', async function() {
    await ensureLazyLoaded();
    const menu = productSpecificationsList.$.sharedMenu.get();
    assertFalse(menu.open);

    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(4, items.length);

    const secondItem = items[1]!;
    secondItem.dispatchEvent(new CustomEvent('item-menu-open', {
      bubbles: true,
      composed: true,
      detail: {target: secondItem, uuid: {value: 'ex2'}},
    }));
    assertTrue(menu.open);

    const button = menu.querySelector('button');
    assertTrue(!!button);
    const buttonText = button!.textContent;
    assertTrue(!!buttonText);
    assertEquals('Remove from tables', buttonText.trim());
  });

  test('clicking remove on menu removes the correct uuid', async function() {
    await ensureLazyLoaded();
    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(4, items.length);
    const firstItem = items[0]!;
    firstItem.dispatchEvent(new CustomEvent('item-menu-open', {
      bubbles: true,
      composed: true,
      detail: {target: firstItem, uuid: {value: 'ex1'}},
    }));

    const menu = productSpecificationsList.$.sharedMenu.get();
    const button = menu.querySelector('button');
    assertTrue(!!button);
    button.click();
    assertEquals(
        1, shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
    assertDeepEquals(
        {value: 'ex1'},
        shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[0]);
  });

  test('delete dialog renders', async function() {
    await ensureLazyLoaded();
    const dialog = productSpecificationsList.$.deleteItemDialog.get();
    assertFalse(dialog.open);

    productSpecificationsList.deleteSelectedWithPrompt();

    assertTrue(dialog.open);
    const title = dialog.querySelector('#title');
    const body = dialog.querySelector('#body');
    const cancelButton = dialog.querySelector('.cancel-button');
    const actionButton = dialog.querySelector('.action-button');
    assertTrue(!!title);
    assertTrue(!!body);
    assertTrue(!!cancelButton);
    assertTrue(!!actionButton);
  });

  test('delete dialog cancel button closes dialog', async function() {
    await ensureLazyLoaded();
    const dialog = productSpecificationsList.$.deleteItemDialog.get();
    productSpecificationsList.deleteSelectedWithPrompt();
    const cancelButton = dialog.querySelector<HTMLElement>('.cancel-button');
    assertTrue(!!cancelButton);

    cancelButton.click();

    assertFalse(dialog.open);
  });

  test('delete dialog action button calls delete', async function() {
    await ensureLazyLoaded();

    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    const checkbox0 = items[0]!.$.checkbox as CrCheckboxElement;
    checkbox0.click();
    await checkbox0.updateComplete;
    const checkbox1 = items[1]!.$.checkbox as CrCheckboxElement;
    checkbox1.click();
    await checkbox1.updateComplete;
    assertDeepEquals(
        new Set(['ex1', 'ex2']), productSpecificationsList.selectedItems);

    const dialog = productSpecificationsList.$.deleteItemDialog.get();
    productSpecificationsList.deleteSelectedWithPrompt();
    const actionButton = dialog.querySelector<HTMLElement>('.action-button');
    assertTrue(!!actionButton);
    actionButton.click();

    assertEquals(
        2, shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
    assertDeepEquals(
        {value: 'ex1'},
        shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[0]);
    assertDeepEquals(
        {value: 'ex2'},
        shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[1]);
  });

  test('focus with arrow keys', async () => {
    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');

    const focusGrid = productSpecificationsList.getFocusGridForTesting();
    assertTrue(!!focusGrid);
    assertEquals(4, focusGrid.rows.length);

    const focusedCheckbox = items[0]!.$.checkbox.getFocusableElement();
    focusedCheckbox.focus();
    assertEquals(focusedCheckbox, getDeepActiveElement());
    assertTrue(focusGrid.rows[0]!.isActive());

    // Press the down arrow to focus on the checkbox in the row below.
    pressAndReleaseKeyOn(focusedCheckbox, 40, [], 'ArrowDown');
    const nextFocusedCheckbox = items[1]!.$.checkbox.getFocusableElement();
    assertEquals(nextFocusedCheckbox, getDeepActiveElement());
    assertTrue(focusGrid.rows[1]!.isActive());
    assertFalse(focusGrid.rows[0]!.isActive());

    // Press the right arrow to focus on the link in the same row.
    pressAndReleaseKeyOn(nextFocusedCheckbox, 39, [], 'ArrowRight');
    const focusedLink = items[1]!.$.link;
    assertEquals(focusedLink, getDeepActiveElement());
    assertTrue(focusGrid.rows[1]!.isActive());
    assertFalse(focusGrid.rows[0]!.isActive());
  });


  test('update list in response to observers', async () => {
    callbackRouterRemote.onProductSpecificationsSetUpdated({
      name: 'example1',
      urls: [{url: 'dot com 1'}, {url: 'dot com 2'}],
      uuid: {value: 'ex1'},
    });
    callbackRouterRemote.onProductSpecificationsSetRemoved({value: 'ex2'});
    callbackRouterRemote.onProductSpecificationsSetAdded({
      name: 'example5',
      urls: [{url: 'dot com 5'}, {url: 'dot com 6'}],
      uuid: {value: 'ex5'},
    });
    await flushTasks();

    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(4, items.length);
    assertDeepEquals(
        {
          name: 'example1',
          uuid: {value: 'ex1'},
          urls: [{url: 'dot com 1'}, {url: 'dot com 2'}],
        },
        items[0]!.item);
    assertDeepEquals(
        {
          name: 'example3',
          uuid: {value: 'ex3'},
          urls: [{url: 'dot com 3a'}, {url: 'dot com 3b'}],
        },
        items[1]!.item);
    assertDeepEquals(
        {
          name: 'example5',
          uuid: {value: 'ex5'},
          urls: [{url: 'dot com 5'}, {url: 'dot com 6'}],
        },
        items[3]!.item);
  });

  test('shift checkbox causes multi select', async function() {
    await ensureLazyLoaded();
    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertDeepEquals(new Set(), productSpecificationsList.selectedItems);

    const firstItem = items[1]!;
    firstItem.dispatchEvent(new CustomEvent('product-spec-item-select', {
      bubbles: true,
      composed: true,
      detail: {
        checked: true,
        shiftKey: false,
        uuid: 'ex2',
        index: 1,
      },
    }));

    const lastItem = items[3]!;
    lastItem.dispatchEvent(new CustomEvent('product-spec-item-select', {
      bubbles: true,
      composed: true,
      detail: {
        checked: true,
        shiftKey: true,
        uuid: 'ex4',
        index: 3,
      },
    }));

    await flushTasks();
    assertDeepEquals(
        new Set(['ex2', 'ex3', 'ex4']),
        productSpecificationsList.selectedItems);
  });

  test('search term changed', async function() {
    await ensureLazyLoaded();
    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(4, items.length);
    initProductSets();

    productSpecificationsList.searchTerm = 'example2';
    await flushTasks();
    const newItems = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');

    assertEquals(1, newItems!.length);
    assertDeepEquals('example2', newItems[0]!.item.name);
  });

  test('empty message renders when list empty', async function() {
    // Reset shoppingAPI to return no product sets.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    shoppingServiceApi.reset();
    shoppingServiceApi.setResultFor(
        'getAllProductSpecificationsSets', Promise.resolve({sets: []}));
    productSpecificationsList =
        document.createElement('product-specifications-lists');
    document.body.appendChild(productSpecificationsList);
    await ensureLazyLoaded();
    await flushTasks();

    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(0, items.length);
    const emptyMessage = productSpecificationsList.shadowRoot!.querySelector(
        '.centered-message');
    assertTrue(!!emptyMessage);
  });

  test('sync button click and message when not synced', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    shoppingServiceApi.reset();
    initProductSets();
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
    shoppingServiceApi.setResultFor(
        'getAllProductSpecificationsSets', Promise.resolve({sets: []}));
    shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);

    productSpecificationsList =
        document.createElement('product-specifications-lists');
    document.body.appendChild(productSpecificationsList);
    await ensureLazyLoaded();
    await flushTasks();

    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(0, items.length);
    const imageTextContainer =
        productSpecificationsList.shadowRoot!.querySelector<HTMLElement>(
            '#sync-or-error-message-picture-and-text-container');
    assertTrue(!!imageTextContainer);
    assertFalse(imageTextContainer.hidden);
    const syncButton =
        productSpecificationsList.shadowRoot!.querySelector<CrButtonElement>(
            '#turn-on-sync-button');
    assertTrue(!!syncButton);

    syncButton.click();
    shoppingServiceApi.whenCalled('showSyncSetupFlow');
  });

  test('error message displays', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    shoppingServiceApi.reset();
    initProductSets();
    shoppingServiceApi.setResultFor(
        'getProductSpecificationsFeatureState', Promise.resolve({
          state: {
            isSyncingTabCompare: true,
            canLoadFullPageUi: true,
            canManageSets: false,
            canFetchData: false,
            isAllowedForEnterprise: false,
          },
        }));
    shoppingServiceApi.setResultFor(
        'getAllProductSpecificationsSets', Promise.resolve({sets: []}));
    shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);

    productSpecificationsList =
        document.createElement('product-specifications-lists');
    document.body.appendChild(productSpecificationsList);
    await ensureLazyLoaded();
    await flushTasks();
    shoppingServiceApi.whenCalled('getProductSpecificationsFeatureState');

    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(0, items.length);
    const imageTextContainer =
        productSpecificationsList.shadowRoot!.querySelector<HTMLElement>(
            '#sync-or-error-message-picture-and-text-container');
    assertTrue(!!imageTextContainer);
    assertFalse(imageTextContainer.hidden);
    const errorMessage =
        productSpecificationsList.shadowRoot!.querySelector<HTMLElement>(
            '#error-message');
    assertTrue(!!errorMessage);
    assertFalse(errorMessage.hidden);
  });

  test('window focus uses new state', async function() {
    const displayList =
        productSpecificationsList.shadowRoot!.querySelector<HTMLElement>(
            '#product-list-padding-container');

    assertTrue(!!displayList);
    assertFalse(displayList.hidden);

    shoppingServiceApi.setResultFor('getCallbackRouter', callbackRouter);
    await shoppingServiceApi.setResultFor(
        'getProductSpecificationsFeatureState', Promise.resolve({
          state: {
            isSyncingTabCompare: false,
            canLoadFullPageUi: true,
            canManageSets: false,
            canFetchData: false,
            isAllowedForEnterprise: false,
            isSignedIn: false,
          },
        }));
    window.dispatchEvent(new Event('focus'));
    await flushTasks();

    assertTrue(!!displayList);
    assertTrue(displayList.hidden);
  });
});
