// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, DriveProxy, driveV2Descriptor} from 'chrome://new-tab-page/new_tab_page.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertEquals, assertTrue} from 'chrome://test/chai_assert.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {isVisible} from 'chrome://test/test_util.js';

suite('NewTabPageModulesDriveModuleTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  setup(() => {
    document.body.innerHTML = '';
    handler =
        installMock(drive.mojom.DriveHandlerRemote, DriveProxy.setHandler);
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
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module = assert(await driveV2Descriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getFiles');
    $$(module, '#fileRepeat').render();
    const items = Array.from(module.shadowRoot.querySelectorAll('.file'));
    const urls = module.shadowRoot.querySelectorAll('.file');

    assertTrue(isVisible(module.$.files));
    assertTrue(!!module);
    assertEquals(2, items.length);
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
    assertEquals('https://foo.com/', urls[0].href);
    assertEquals('https://bar.com/', urls[1].href);
  });

  test('empty module shows without data', async () => {
    handler.setResultFor('getFiles', Promise.resolve({files: []}));

    const module = await driveV2Descriptor.initialize(0);
    await handler.whenCalled('getFiles');
    assertTrue(!!module);
  });

  test('module has height of 86 with only one file', async () => {
    const data = {
      files: [
        {
          title: 'Abc',
        },
      ]
    };
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module = assert(await driveV2Descriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getFiles');
    $$(module, '#fileRepeat').render();

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
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module = assert(await driveV2Descriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getFiles');
    $$(module, '#fileRepeat').render();

    assertEquals(142, module.offsetHeight);
  });


  test('clicking the info button opens the ntp info dialog box', async () => {
    // Arrange.
    const data = {
      files: [
        {
          justificationText: 'Edited yesterday',
          title: 'Abc',
          id: '012',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://abc.com'},
        },
      ]
    };
    handler.setResultFor('getFiles', Promise.resolve(data));
    const driveModule = assert(await driveV2Descriptor.initialize(0));
    document.body.append(driveModule);

    // Act.
    const infoEvent = new Event('info-button-click');
    $$(driveModule, 'ntp-module-header').dispatchEvent(infoEvent);

    // Assert.
    assertTrue(!!$$(driveModule, 'ntp-info-dialog'));
  });

  test(
      'clicking the disable button sets the correct toast message',
      async () => {
        // Arrange.
        const data = {
          files: [
            {
              justificationText: 'Edited yesterday',
              title: 'Abc',
              id: '012',
              mimeType: 'application/vnd.google-apps.presentation',
              itemUrl: {url: 'https://abc.com'},
            },
          ]
        };
        handler.setResultFor('getFiles', Promise.resolve(data));
        const driveModule = assert(await driveV2Descriptor.initialize(0));
        document.body.append(driveModule);

        // Act.
        const disable = {event: null};
        driveModule.addEventListener(
            'disable-module', (e) => disable.event = e);
        const disableEvent = new Event('disable-button-click');
        $$(driveModule, 'ntp-module-header').dispatchEvent(disableEvent);

        // Assert.
        assertEquals(
            'You won\'t see Drive files again on this page',
            disable.event.detail.message);
      });

  test('backend is notified when module is dismissed or restored', async () => {
    // Arrange.
    const data = {
      files: [
        {
          justificationText: 'Edited yesterday',
          title: 'Abc',
          id: '012',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://abc.com'},
        },
      ]
    };
    handler.setResultFor('getFiles', Promise.resolve(data));
    const driveModule = assert(await driveV2Descriptor.initialize(0));
    document.body.append(driveModule);

    // Act.
    const dismiss = {event: null};
    driveModule.addEventListener('dismiss-module', (e) => dismiss.event = e);
    const dismissEvent = new Event('dismiss-button-click');
    $$(driveModule, 'ntp-module-header').dispatchEvent(dismissEvent);

    // Assert.
    assertEquals('Files hidden', dismiss.event.detail.message);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    dismiss.event.detail.restoreCallback();

    // Assert.
    assertEquals(1, handler.getCallCount('restoreModule'));
  });
});
