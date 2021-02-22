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
    const data = {
      files: [
        {
          justificationText: 'Edited last week',
          title: 'Foo',
          id: '123',
          type: drive.mojom.FileType.kDoc,
        },
        {
          justificationText: 'Edited today',
          title: 'Bar',
          id: '234',
          type: drive.mojom.FileType.kSheet,
        },
        {
          justificationText: 'Created today',
          title: 'Caz',
          id: '345',
          type: drive.mojom.FileType.kOther,
        }
      ]
    };
    testProxy.handler.setResultFor('getFiles', Promise.resolve(data));

    await driveDescriptor.initialize();
    const module = driveDescriptor.element;
    document.body.append(module);
    await testProxy.handler.whenCalled('getFiles');
    module.$.fileRepeat.render();

    const items = Array.from(module.shadowRoot.querySelectorAll('.file'));
    assertTrue(!!module);
    assertTrue(isVisible(module.$.files));
    assertEquals(3, items.length);
    assertEquals('Bar', items[1].querySelector('.file-title').textContent);
    assertEquals(
        'Edited today',
        items[1].querySelector('.file-description').textContent);
    const urls = module.shadowRoot.querySelectorAll('.file');
    assertEquals(
        'https://docs.google.com/document/d/123/edit?usp=drive_web',
        urls[0].href);
    assertEquals(
        'https://docs.google.com/spreadsheets/d/234/edit?usp=drive_web',
        urls[1].href);
    assertEquals(
        'https://drive.google.com/file/d/345/view?usp=drive_web', urls[2].href);
  });

  test('documents do not show without data', async () => {
    testProxy.handler.setResultFor('getFiles', Promise.resolve({files: []}));

    await driveDescriptor.initialize();
    await testProxy.handler.whenCalled('getFiles');
    assertFalse(!!driveDescriptor.element);
  });
});
