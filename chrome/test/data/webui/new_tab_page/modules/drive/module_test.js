// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {driveDescriptor, DriveProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise, isVisible} from 'chrome://test/test_util.m.js';

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
          mimeType: 'application/vnd.google-apps.spreadsheet',
          url: {url: 'https://foo.com'},
        },
        {
          justificationText: 'Edited today',
          title: 'Bar',
          id: '234',
          mimeType: 'application/vnd.google-apps.document',
          url: {url: 'https://bar.com'},
        },
        {
          justificationText: 'Created today',
          title: 'Caz',
          id: '345',
          mimeType: 'application/vnd.google-apps.presentation',
          url: {url: 'https://caz.com'},
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
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/128/type/application/vnd.google-apps.spreadsheet',
        items[0].querySelector('img').autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/128/type/application/vnd.google-apps.document',
        items[1].querySelector('img').autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/128/type/application/vnd.google-apps.presentation',
        items[2].querySelector('img').autoSrc);
    const urls = module.shadowRoot.querySelectorAll('.file');
    assertEquals('https://foo.com/', urls[0].href);
    assertEquals('https://bar.com/', urls[1].href);
    assertEquals('https://caz.com/', urls[2].href);
  });

  test('documents are hidden at narrower widths', async () => {
    const repeat = (n, fn) => Array(n).fill(0).map(fn);
    testProxy.handler.setResultFor('getFiles', Promise.resolve({
      files: repeat(3, () => ({
                         justification: 'edited',
                         title: 'foo',
                         id: '123',
                         mimeType: 'application/vnd.google-apps.document',
                       }))
    }));

    await driveDescriptor.initialize();
    const module = driveDescriptor.element;
    document.body.append(module);
    module.$.fileRepeat.render();

    const items = Array.from(module.shadowRoot.querySelectorAll('.file'));
    // Setting the module height ensures that the IntersectionRatio
    // can only be made less than one by the width.
    module.style.height = '260px';
    const countHidden = async (width, count) => {
      module.style.width = width;
      var waitForEvent = eventToPromise('change-visibility', module);
      await waitForEvent;
      assertEquals(
          count, items.filter(el => el.style.visibility === 'hidden').length);
    };
    await countHidden('200px', 2);
    await countHidden('600px', 0);
    await countHidden('400px', 1);
  });
  test('documents do not show without data', async () => {
    testProxy.handler.setResultFor('getFiles', Promise.resolve({files: []}));

    await driveDescriptor.initialize();
    await testProxy.handler.whenCalled('getFiles');
    assertFalse(!!driveDescriptor.element);
  });
});
