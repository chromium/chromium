// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {driveDescriptor, DriveProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

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
    testProxy.handler.setResultFor(
        'getTestString', Promise.resolve({dataItem: 'test string'}));
    await driveDescriptor.initialize();
    const module = driveDescriptor.element;
    assertTrue(!!module);
    await testProxy.handler.whenCalled('getTestString');
    assertTrue(!!driveDescriptor.element);
  });
});
