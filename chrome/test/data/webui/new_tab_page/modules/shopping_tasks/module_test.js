// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {shoppingTasksDescriptor, ShoppingTasksHandlerProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

suite('NewTabPageModulesShoppingTasksModuleTest', () => {
  /**
   * @implements {ShoppingTasksHandlerProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = TestBrowserProxy.fromClass(ShoppingTasksHandlerProxy);
    testProxy.handler = TestBrowserProxy.fromClass(
        shoppingTasks.mojom.ShoppingTasksHandlerRemote);
    ShoppingTasksHandlerProxy.instance_ = testProxy;
  });

  test('creates no module if no task', async () => {
    // Arrange.
    testProxy.handler.setResultFor(
        'getPrimaryShoppingTask', Promise.resolve({shoppingTask: null}));

    // Act.
    await shoppingTasksDescriptor.initialize();

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('getPrimaryShoppingTask'));
    assertEquals(null, shoppingTasksDescriptor.element);
  });

  test('creates module if task', async () => {
    // Arrange.
    const shoppingTask = {
      title: 'Hello world',
      products: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          price: '1 gazillion dollars',
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
          price: '2 gazillion dollars',
          info: 'bar info',
          targetUrl: {url: 'https://bar.com'},
        },
      ],
      relatedSearches: [
        {
          text: 'baz',
          targetUrl: {url: 'https://baz.com'},
        },
        {
          text: 'blub',
          targetUrl: {url: 'https://blub.com'},
        },
      ],
    };
    testProxy.handler.setResultFor(
        'getPrimaryShoppingTask', Promise.resolve({shoppingTask}));

    // Act.
    await shoppingTasksDescriptor.initialize();
    const module = shoppingTasksDescriptor.element;
    document.body.append(module);
    module.$.productsRepeat.render();
    module.$.relatedSearchesRepeat.render();

    // Assert.
    const products = Array.from(module.shadowRoot.querySelectorAll('.product'));
    const pills = Array.from(module.shadowRoot.querySelectorAll('.pill'));
    assertEquals(1, testProxy.handler.getCallCount('getPrimaryShoppingTask'));
    assertEquals(2, products.length);
    assertEquals(2, pills.length);
    assertEquals('https://foo.com/', products[0].href);
    assertEquals(
        'https://foo.com/img.png', products[0].querySelector('img').autoSrc);
    assertEquals(
        '1 gazillion dollars', products[0].querySelector('.price').innerText);
    assertEquals('foo', products[0].querySelector('.name').innerText);
    assertEquals('foo', products[0].querySelector('.name').title);
    assertEquals('foo info', products[0].querySelector('.info').innerText);
    assertEquals('https://bar.com/', products[1].href);
    assertEquals(
        'https://bar.com/img.png', products[1].querySelector('img').autoSrc);
    assertEquals(
        '2 gazillion dollars', products[1].querySelector('.price').innerText);
    assertEquals('bar', products[1].querySelector('.name').innerText);
    assertEquals('bar', products[1].querySelector('.name').title);
    assertEquals('bar info', products[1].querySelector('.info').innerText);
    assertEquals('https://baz.com/', pills[0].href);
    assertEquals('baz', pills[0].querySelector('.search-text').innerText);
    assertEquals('https://blub.com/', pills[1].href);
    assertEquals('blub', pills[1].querySelector('.search-text').innerText);
  });
});
