// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DriveProxy, driveV2Descriptor} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {isVisible} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesDriveModuleTest', () => {
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();
    testProxy = TestBrowserProxy.fromClass(DriveProxy);
    testProxy.handler =
        TestBrowserProxy.fromClass(drive.mojom.DriveHandlerRemote);
    DriveProxy.setInstance(testProxy);
  });

  test('module appears on render', async () => {
    const data = {
      files: [
        {
          justificationText: 'Edited last week',
          title: 'Foo',
          id: '123',
          mimeType: 'application/vnd.google-apps.spreadsheet',
          itemUrl: {url: 'https://foo.com'},
        },
        {
          justificationText: 'Edited yesterday',
          title: 'Bar',
          id: '234',
          mimeType: 'application/vnd.google-apps.document',
          itemUrl: {url: 'https://bar.com'},
        },
        {
          justificationText: 'Created today',
          title: 'Caz',
          id: '345',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://caz.com'},
        }
      ]
    };
    testProxy.handler.setResultFor('getFiles', Promise.resolve(data));

    const module = await driveV2Descriptor.initialize();
    document.body.append(module);
    await testProxy.handler.whenCalled('getFiles');
    module.$.fileRepeat.render();
    const items = Array.from(module.shadowRoot.querySelectorAll('.file'));
    const urls = module.shadowRoot.querySelectorAll('.file');

    assertTrue(isVisible(module.$.files));
    assertTrue(!!module);
    assertEquals(3, items.length);
    assertEquals('Bar', items[1].querySelector('.file-title').textContent);
    assertEquals(
        'Edited yesterday',
        items[1].querySelector('.file-description').textContent);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.spreadsheet',
        items[0].querySelector('.file-icon').autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.document',
        items[1].querySelector('.file-icon').autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.presentation',
        items[2].querySelector('.file-icon').autoSrc);
    assertEquals('https://foo.com/', urls[0].href);
    assertEquals('https://bar.com/', urls[1].href);
    assertEquals('https://caz.com/', urls[2].href);
  });

  test('documents do not show without data', async () => {
    testProxy.handler.setResultFor('getFiles', Promise.resolve({files: []}));

    const module = await driveV2Descriptor.initialize();
    await testProxy.handler.whenCalled('getFiles');
    assertTrue(!module);
  });

  test('module has height of 86 with only one file', async () => {
    const data = {
      files: [
        {
          title: 'Abc',
        },
      ]
    };
    testProxy.handler.setResultFor('getFiles', Promise.resolve(data));

    const module = await driveV2Descriptor.initialize();
    document.body.append(module);
    await testProxy.handler.whenCalled('getFiles');
    module.$.fileRepeat.render();

    assertEquals(86, module.offsetHeight);
  });

  test('module has height of 142 with two files', async () => {
    const data = {
      files: [
        {
          title: 'Abc',
        },
        {
          title: 'Def',
        },
      ]
    };
    testProxy.handler.setResultFor('getFiles', Promise.resolve(data));

    const module = await driveV2Descriptor.initialize();
    document.body.append(module);
    await testProxy.handler.whenCalled('getFiles');
    module.$.fileRepeat.render();

    assertEquals(142, module.offsetHeight);
  });

  test('module has height of 198 with 3 files', async () => {
    const data = {
      files: [
        {
          title: 'Abc',
        },
        {
          title: 'Def',
        },
        {
          title: 'Ghi',
        },
      ]
    };
    testProxy.handler.setResultFor('getFiles', Promise.resolve(data));

    const module = await driveV2Descriptor.initialize();
    document.body.append(module);
    await testProxy.handler.whenCalled('getFiles');
    module.$.fileRepeat.render();

    assertEquals(198, module.offsetHeight);
  });
});
