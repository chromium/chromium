// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/app.js';

import type {ProductSpecificationsElement} from 'chrome://compare/app.js';
import {Router} from 'chrome://compare/router.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {$$, assertNotStyle, assertStyle} from './test_support.js';

suite('AppTest', () => {
  let appElement: ProductSpecificationsElement;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);
  const router = TestMock.fromClass(Router);

  async function createAppElement(): Promise<ProductSpecificationsElement> {
    appElement = document.createElement('product-specifications-app');
    document.body.appendChild(appElement);
    return appElement;
  }

  async function createAppElementWithUrls(
      urls: string[] = ['https://example.com/', 'https://example2.com/']) {
    router.setResultFor('getCurrentQuery', `?urls=${JSON.stringify(urls)}`);
    createAppElement();
  }

  setup(async () => {
    shoppingServiceApi.reset();
    router.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
    Router.setInstance(router);
  });

  test('calls shopping service if there are url params', () => {
    createAppElementWithUrls();

    assertEquals(1, router.getCallCount('getCurrentQuery'));
    assertEquals(
        1, shoppingServiceApi.getCallCount('getProductSpecificationsForUrls'));
  });

  test('parses product urls', async () => {
    const testUrls = ['https://example.com/', 'https://example2.com/'];
    createAppElementWithUrls(testUrls);

    const urls =
        await shoppingServiceApi.whenCalled('getProductSpecificationsForUrls');
    assertTrue(Array.isArray(urls));
    assertEquals(2, urls.length);
    assertArrayEquals(testUrls, urls.map(u => u.url));
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

  suite('EmptyState', () => {
    test('shows empty state if app loads without urls', () => {
      router.setResultFor('getCurrentQuery', '');
      createAppElement();

      assertNotStyle($$(appElement, '#empty')!, 'display', 'none');
      assertStyle($$(appElement, '#specs')!, 'display', 'none');
    });

    test('hides empty state if app loads with urls', () => {
      createAppElementWithUrls();

      assertStyle($$(appElement, '#empty')!, 'display', 'none');
      assertNotStyle($$(appElement, '#specs')!, 'display', 'none');
    });
  });
});
