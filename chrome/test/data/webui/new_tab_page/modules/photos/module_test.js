// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {photosDescriptor, PhotosProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {isVisible} from 'chrome://test/test_util.js';

suite('NewTabPageModulesPhotosModuleTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  setup(() => {
    PolymerTest.clearBody();
    handler =
        installMock(photos.mojom.PhotosHandlerRemote, PhotosProxy.setHandler);
  });

  test('module appears on render', async () => {
    const data = {
      memories: [{title: 'Title 1', id: 'key1'}, {title: 'Title 2', id: 'key2'}]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));

    const module = await photosDescriptor.initialize();
    document.body.append(module);
    await handler.whenCalled('getMemories');
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
    handler.setResultFor('getMemories', Promise.resolve({memories: []}));

    const module = await photosDescriptor.initialize();
    await handler.whenCalled('getMemories');
    assertFalse(!!module);
  });
});
