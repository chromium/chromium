// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar/toolbar.js';

import {ActionId, CategoryId} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import type {Action, Category, CustomizeToolbarClientRemote, CustomizeToolbarHandlerInterface} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import {CustomizeToolbarClientCallbackRouter, CustomizeToolbarHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import {CustomizeToolbarApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar/customize_toolbar_api_proxy.js';
import type {ToolbarElement} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar/toolbar.js';
import {WindowProxy} from 'chrome://customize-chrome-side-panel.top-chrome/window_proxy.js';
import {assertEquals, assertFalse, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('CustomizerToolbarTest', () => {
  let handler: TestMock<CustomizeToolbarHandlerInterface>;
  let customizeToolbarCallbackRouterRemote: CustomizeToolbarClientRemote;
  let toolbarElement: ToolbarElement;
  let windowProxy: TestMock<WindowProxy>;

  async function createToolbarElement(
      actions: Action[] = [],
      categories: Category[] = []): Promise<ToolbarElement> {
    handler.setResultFor('listActions', Promise.resolve({actions}));
    handler.setResultFor('listCategories', Promise.resolve({categories}));
    handler.setResultFor(
        'getIsCustomized', Promise.resolve({customized: false}));
    toolbarElement = document.createElement('customize-chrome-toolbar');
    document.body.appendChild(toolbarElement);
    return toolbarElement;
  }

  async function createToolbarElementWithBasicData() {
    createToolbarElement(
        [
          {
            id: ActionId.kHome,
            displayName: 'Home',
            pinned: true,
            category: CategoryId.kNavigation,
            iconUrl: {url: 'https://example.com/foo_1.png'},
          },
          {
            id: ActionId.kShowPasswordManager,
            displayName: 'Show Password Manager',
            pinned: false,
            category: CategoryId.kYourChrome,
            iconUrl: {url: 'https://example.com/foo_1.png'},
          },
        ],
        [
          {id: CategoryId.kNavigation, displayName: 'Navigation'},
          {id: CategoryId.kYourChrome, displayName: 'Your Chrome'},
        ]);
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('onLine', true);
    handler = installMock(
        CustomizeToolbarHandlerRemote,
        (mock: CustomizeToolbarHandlerRemote) =>
            CustomizeToolbarApiProxy.setInstance(
                mock, new CustomizeToolbarClientCallbackRouter()));
    customizeToolbarCallbackRouterRemote =
        CustomizeToolbarApiProxy.getInstance()
            .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  test('toolbar element added to side panel', () => {
    createToolbarElement();
    assertTrue(document.body.contains(toolbarElement));
  });

  test('clicking back button creates event', async () => {
    createToolbarElement();
    const eventPromise = eventToPromise('back-click', toolbarElement);
    toolbarElement.$.heading.getBackButton().click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('actions are fetched from the backend', () => {
    createToolbarElement();
    assertEquals(1, handler.getCallCount('listActions'));
  });

  test('categories are fetched from the backend', () => {
    createToolbarElement();
    assertEquals(1, handler.getCallCount('listCategories'));
  });

  test('actions and categories are structured correctly', async () => {
    createToolbarElementWithBasicData();
    await microtasksFinished();

    const categories =
        toolbarElement.shadowRoot!.querySelectorAll('.category-title');
    assertEquals(2, categories.length);

    const actions =
        toolbarElement.shadowRoot!.querySelectorAll('.toggle-container');
    assertEquals(2, actions.length);

    const navigationCategory = categories[0];
    assertTrue(!!navigationCategory);
    assertEquals('Navigation', navigationCategory.textContent!.trim());
    const homeAction = actions[0];
    assertTrue(!!homeAction);
    assertEquals(
        'Home', homeAction.querySelector('.toggle-title')!.textContent!.trim());

    const yourChromeCategory = categories[1];
    assertTrue(!!yourChromeCategory);
    assertEquals('Your Chrome', yourChromeCategory.textContent!.trim());
    const showPasswordManagerAction = actions[1];
    assertTrue(!!showPasswordManagerAction);
    assertEquals(
        'Show Password Manager',
        showPasswordManagerAction.querySelector(
                                     '.toggle-title')!.textContent!.trim());

    const children = Array.from(toolbarElement.$.pinningSelectionCard.children);
    assertGE(
        children.indexOf(homeAction), children.indexOf(navigationCategory));
    assertGE(
        children.indexOf(yourChromeCategory), children.indexOf(homeAction));
    assertGE(
        children.indexOf(showPasswordManagerAction),
        children.indexOf(yourChromeCategory));
  });

  test('action has icon', async () => {
    createToolbarElementWithBasicData();
    await microtasksFinished();

    const homeAction =
        toolbarElement.shadowRoot!.querySelector('.toggle-container');
    assertTrue(!!homeAction);
    const iconUrl = homeAction.querySelector('img')!.src;
    assertEquals('https://example.com/foo_1.png', iconUrl);
  });

  test('pinning via toggle notifies backend', async () => {
    createToolbarElementWithBasicData();
    await microtasksFinished();

    const homeAction =
        toolbarElement.shadowRoot!.querySelector('.toggle-container');
    assertTrue(!!homeAction);
    const homeActionToggle = homeAction.querySelector('cr-toggle');
    assertTrue(!!homeActionToggle);

    homeActionToggle.click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('pinAction'));
    assertEquals(ActionId.kHome, handler.getArgs('pinAction')[0][0]);
    assertEquals(false, handler.getArgs('pinAction')[0][1]);
  });

  test('pinning via backend updates toggle state', async () => {
    createToolbarElementWithBasicData();
    await microtasksFinished();

    const homeAction =
        toolbarElement.shadowRoot!.querySelector('.toggle-container');
    assertTrue(!!homeAction);
    const homeActionToggle = homeAction.querySelector('cr-toggle');
    assertTrue(!!homeActionToggle);

    assertTrue(homeActionToggle.checked);

    customizeToolbarCallbackRouterRemote.setActionPinned(ActionId.kHome, false);
    await customizeToolbarCallbackRouterRemote.$.flushForTesting();

    assertFalse(homeActionToggle.checked);
  });
});
