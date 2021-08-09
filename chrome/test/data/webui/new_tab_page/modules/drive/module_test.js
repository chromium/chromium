// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, driveDescriptor, DriveProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
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
          justificationText: 'Edited today',
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

    const module = await driveDescriptor.initialize();
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
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.spreadsheet',
        items[0].querySelector('.file-icon').autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.document',
        items[1].querySelector('.file-icon').autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.presentation',
        items[2].querySelector('.file-icon').autoSrc);
    const urls = module.shadowRoot.querySelectorAll('.file');
    assertEquals('https://foo.com/', urls[0].href);
    assertEquals('https://bar.com/', urls[1].href);
    assertEquals('https://caz.com/', urls[2].href);
  });

  test('documents do not show without data', async () => {
    testProxy.handler.setResultFor('getFiles', Promise.resolve({files: []}));

    const module = await driveDescriptor.initialize();
    await testProxy.handler.whenCalled('getFiles');
    assertFalse(!!module);
  });

  test('backend is notified when module is dismissed or restored', async () => {
    // Arrange.
    const data = {
      files: [
        {
          justificationText: '',
          title: '',
          id: '',
          mimeType: '',
          itemUrl: {url: ''},
        },
      ]
    };
    testProxy.handler.setResultFor('getFiles', Promise.resolve(data));
    const moduleElement = await driveDescriptor.initialize();
    document.body.append(moduleElement);

    // Act.
    const dismiss = {event: null};
    moduleElement.addEventListener('dismiss-module', (e) => dismiss.event = e);
    $$(moduleElement, 'ntp-module-header')
        .dispatchEvent(new Event('dismiss-button-click'));

    // Assert.
    assertEquals('Files hidden', dismiss.event.detail.message);
    assertEquals(1, testProxy.handler.getCallCount('dismissModule'));

    // Act.
    dismiss.event.detail.restoreCallback();

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('restoreModule'));
  });

  test('info button click opens info dialog', async () => {
    // Arrange.
    const data = {
      files: [
        {
          justificationText: '',
          title: '',
          id: '',
          mimeType: '',
          itemUrl: {url: ''},
        },
      ]
    };
    testProxy.handler.setResultFor('getFiles', Promise.resolve(data));
    const module = await driveDescriptor.initialize();
    document.body.append(module);

    // Act.
    $$(module, 'ntp-module-header')
        .dispatchEvent(new Event('info-button-click'));

    // Assert.
    assertTrue(!!$$(module, 'ntp-info-dialog'));
  });
});
