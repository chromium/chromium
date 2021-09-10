// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, dummyDescriptor, FooProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {isVisible} from 'chrome://test/test_util.js';

suite('NewTabPageModulesDummyModuleTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  setup(() => {
    document.body.innerHTML = '';

    handler = installMock(foo.mojom.FooHandlerRemote, FooProxy.setHandler);
    handler.setResultFor('getData', Promise.resolve({data: []}));
  });

  test('creates module with data', async () => {
    // Act.
    const data = [
      {
        label: 'item1',
        value: 'foo',
        imageUrl: 'foo.com',
      },
      {
        label: 'item2',
        value: 'bar',
        imageUrl: 'bar.com',
      },
      {
        label: 'item3',
        value: 'baz',
        imageUrl: 'baz.com',
      },
    ];
    handler.setResultFor('getData', Promise.resolve({data}));
    const module = await dummyDescriptor.initialize(0);
    assert(module);
    document.body.append(module);
    $$(module, '#tileList').render();

    // Assert.
    assertTrue(isVisible(module.$.tiles));
    const tiles = module.shadowRoot.querySelectorAll('#tiles .tile-item');
    assertEquals(3, tiles.length);
    assertEquals('item3', tiles[2].getAttribute('title'));
    assertEquals('baz', tiles[2].querySelector('span').textContent);
    assertEquals('baz.com', tiles[2].querySelector('img').autoSrc);
  });

  test('creates module without data', async () => {
    // Act.
    const module = await dummyDescriptor.initialize(0);
    assert(module);
    document.body.append(module);
    $$(module, '#tileList').render();

    // Assert.
    assertFalse(isVisible(module.$.tiles));
    const tiles = module.shadowRoot.querySelectorAll('#tiles .tile-item');
    assertEquals(0, tiles.length);
  });
});
