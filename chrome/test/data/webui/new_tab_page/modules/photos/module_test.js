// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, photosDescriptor, PhotosProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

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

  test('module does not show without data', async () => {
    testProxy.handler.setResultFor(
        'getMemories', Promise.resolve({memories: []}));

    const module = await photosDescriptor.initialize();
    await testProxy.handler.whenCalled('getMemories');
    assertFalse(!!module);
  });
});
