// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {FooHandlerRemote} from 'chrome://new-tab-page/foo.mojom-webui.js';
import {DummyModuleElement, dummyV2Descriptor, FooProxy} from 'chrome://new-tab-page/lazy_load.js';
import {CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesDummyModuleTest', () => {
  let handler: TestBrowserProxy<FooHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = installMock(FooHandlerRemote, FooProxy.setHandler);
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
    const module = await dummyV2Descriptor.initialize(0) as DummyModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    module.$.tileList.render();

    // Assert.
    assertTrue(isVisible(module.$.tiles));
    const tiles = module.shadowRoot!.querySelectorAll('#tiles .tile-item');
    assertEquals(3, tiles.length);
    assertEquals('item3', tiles[2]!.getAttribute('title'));
    assertEquals('baz', tiles[2]!.querySelector('span')!.textContent);
    assertEquals(
        'baz.com', tiles[2]!.querySelector<CrAutoImgElement>('img')!.autoSrc);
  });

  test('creates module without data', async () => {
    // Act.
    const module = await dummyV2Descriptor.initialize(0) as DummyModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    module.$.tileList.render();

    // Assert.
    assertFalse(isVisible(module.$.tiles));
    const tiles = module.shadowRoot!.querySelectorAll('#tiles .tile-item');
    assertEquals(0, tiles.length);
  });
});
