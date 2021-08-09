// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, photosDescriptor, PhotosProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

suite('NewTabPageModulesPhotosModuleTest', () => {
  /**
   * @implements {PhotosProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();
    testProxy = TestBrowserProxy.fromClass(PhotosProxy);
    testProxy.handler =
        TestBrowserProxy.fromClass(photos.mojom.PhotosHandlerRemote);
    PhotosProxy.setInstance(testProxy);
  });

  test('module appears on render', async () => {
    const data = {
      memories: [{title: 'Title 1', id: 'key1'}, {title: 'Title 2', id: 'key2'}]
    };
    testProxy.handler.setResultFor('getMemories', Promise.resolve(data));

    const module = await photosDescriptor.initialize();
    document.body.append(module);
    await testProxy.handler.whenCalled('getMemories');
    module.$.memoryRepeat.render();

    const items = Array.from(module.shadowRoot.querySelectorAll('.memory'));
    assertTrue(!!module);
    assertTrue(isVisible(module.$.memories));
    assertEquals(2, items.length);
    assertEquals(
        'Title 1', items[0].querySelector('.memory-title').textContent);
    assertEquals(
        'Title 2', items[1].querySelector('.memory-title').textContent);
  });

  test('module does not show without data', async () => {
    testProxy.handler.setResultFor(
        'getMemories', Promise.resolve({memories: []}));

    const module = await photosDescriptor.initialize();
    await testProxy.handler.whenCalled('getMemories');
    assertFalse(!!module);
  });
});
