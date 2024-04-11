// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/product_specifications/app.js';

import type {ProductSpecificationsElement} from 'chrome://compare/product_specifications/app.js';
import {Router} from 'chrome://compare/router.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('AppTest', () => {
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);
  const router = TestMock.fromClass(Router);

  async function createApp(): Promise<ProductSpecificationsElement> {
    const app = document.createElement('product-specifications-app');
    document.body.appendChild(app);
    await flushTasks();
    return app;
  }

  setup(async () => {
    shoppingServiceApi.reset();
    router.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
    Router.setInstance(router);
  });

  test('ParsesProductUrls', async () => {
    const testUrls = ['https://example.com/', 'https://example2.com/'];
    router.setResultFor('getCurrentQuery', `?urls=${JSON.stringify(testUrls)}`);
    await createApp();
    const urls =
        await shoppingServiceApi.whenCalled('getProductSpecificationsForUrls');

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertTrue(Array.isArray(urls));
    assertEquals(2, urls.length);
    assertArrayEquals(testUrls, urls.map(u => u.url));
  });

  test('HandlesInvalidRoute', async () => {
    router.setResultFor('getCurrentQuery', `?urls=INVALID_JSON}`);
    await createApp();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        0, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
  });

  test('HandlesMissingRouter', async () => {
    router.setResultFor('getCurrentQuery', '');
    await createApp();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        0, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
  });
});
