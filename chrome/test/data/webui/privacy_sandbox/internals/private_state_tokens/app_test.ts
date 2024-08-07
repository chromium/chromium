// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {PrivateStateTokensAppElement, PrivateStateTokensNavigationElement} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {ItemsToRender, PrivateStateTokensApiBrowserProxyImpl} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrivateStateTokensApiBrowserProxy} from './test_api_proxy.js';
import {dummyIssuerTokenCounts} from './test_data.js';

suite('PrivateStateTokensAppTest', () => {
  let app: PrivateStateTokensAppElement;
  let testProxy: TestPrivateStateTokensApiBrowserProxy;

  setup(async () => {
    testProxy = new TestPrivateStateTokensApiBrowserProxy();
    PrivateStateTokensApiBrowserProxyImpl.setInstance(testProxy);
    testProxy.handler.privateStateTokensCounts = dummyIssuerTokenCounts;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('private-state-tokens-app');
    document.body.appendChild(app);
    app.setNarrowForTesting(false);
    await microtasksFinished();
  });

  test('check initial state', () => {
    assertEquals(1, testProxy.handler.getCallCount('getIssuerTokenCounts'));
    assertTrue(isVisible(app));
    assertTrue(isVisible(app.$.sidebar));
  });

  test('app drawer', async () => {
    app.setNarrowForTesting(true);
    await microtasksFinished();

    assertFalse(app.$.drawer.open);
    const menuButton =
        app.$.toolbar.$.mainToolbar.shadowRoot!.querySelector<HTMLElement>(
            '#menuButton');
    assertTrue(isVisible(menuButton));
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();

    assertTrue(app.$.drawer.open);
    app.$.drawer.close();
    await microtasksFinished();
    assertFalse(isVisible(app.$.drawer));
  });

  test('check rendered item', async () => {
    const container = app.shadowRoot!.querySelector<HTMLElement>('#container')!
                          .querySelector<HTMLElement>('#content');
    assertTrue(!!container);
    const contentContainer =
        container.querySelector<PrivateStateTokensNavigationElement>(
            'private-state-tokens-navigation');
    assertTrue(!!contentContainer);
    const contentContainerChild =
        contentContainer.shadowRoot!.querySelector<HTMLElement>(
            'private-state-tokens-list-container');
    assertEquals(
        app.itemToRender, ItemsToRender.ISSUER_LIST,
        'app.itemToRender is not ISSUER_LIST');
    assertTrue(
        !!contentContainerChild, 'contentContainerChild is null or undefined');
  });
});
