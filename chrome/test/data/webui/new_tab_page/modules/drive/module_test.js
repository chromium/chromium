// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {driveDescriptor, DriveProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {isVisible} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesDriveModuleTest', () => {
  /**
   * @implements {DriveProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();
    testProxy = TestBrowserProxy.fromClass(DriveProxy);
    testProxy.handler =
        TestBrowserProxy.fromClass(drive.mojom.DriveHandlerRemote);
    DriveProxy.instance_ = testProxy;
  });

  test('module appears on render', async () => {
    const titles = ['Foo', 'Bar', 'Caz'];
    testProxy.handler.setResultFor(
        'getDocuments',
        Promise.resolve({documents: titles.map(title => ({title}))}));

    await driveDescriptor.initialize();
    const module = driveDescriptor.element;
    document.body.append(module);
    await testProxy.handler.whenCalled('getDocuments');
    module.$.documentRepeat.render();

    const items = Array.from(module.shadowRoot.querySelectorAll('.document'));
    assertTrue(!!module);
    assertTrue(isVisible(module.$.documents));
    assertEquals(3, items.length);
    assertDeepEquals(titles, items.map(item => item.innerText));
  });

  test('documents do not show without data', async () => {
    testProxy.handler.setResultFor(
        'getDocuments', Promise.resolve({documents: []}));

    await driveDescriptor.initialize();
    await testProxy.handler.whenCalled('getDocuments');
    assertFalse(!!driveDescriptor.element);
  });
});
