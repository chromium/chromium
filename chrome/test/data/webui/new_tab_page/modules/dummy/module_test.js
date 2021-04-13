// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dummyDescriptor, FooProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {isVisible} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesDummyModuleTest', () => {
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = FooProxy.getInstance();
    testProxy.handler = TestBrowserProxy.fromClass(foo.mojom.FooHandlerRemote);
    testProxy.handler.setResultFor('getData', Promise.resolve({data: []}));
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
    testProxy.handler.setResultFor('getData', Promise.resolve({data}));
    await dummyDescriptor.initialize();
    const module = dummyDescriptor.element;
    document.body.append(module);
    module.$.tileList.render();

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
    await dummyDescriptor.initialize();
    const module = dummyDescriptor.element;
    document.body.append(module);
    module.$.tileList.render();

    // Assert.
    assertFalse(isVisible(module.$.tiles));
    const tiles = module.shadowRoot.querySelectorAll('#tiles .tile-item');
    assertEquals(0, tiles.length);
  });
});
