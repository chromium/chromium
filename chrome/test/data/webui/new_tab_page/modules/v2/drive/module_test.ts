// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DriveHandlerRemote} from 'chrome://new-tab-page/drive.mojom-webui.js';
import {DisableModuleEvent, DriveProxy, driveV2Descriptor, DriveV2ModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$, CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

suite('DriveV2Module', () => {
  let handler: TestMock<DriveHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(DriveHandlerRemote, DriveProxy.setHandler);
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
          title: 'Baz',
          id: '345',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://baz.com'},
        },
        {
          justificationText: 'Created yesterday',
          title: 'Qux',
          id: '345',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://qux.com'},
        },
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module =
        await driveV2Descriptor.initialize(0) as DriveV2ModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getFiles');
    module.$.fileRepeat.render();
    const items = Array.from(module.shadowRoot!.querySelectorAll('.file'));
    const urls =
        module.shadowRoot!.querySelectorAll<HTMLAnchorElement>('.file');

    assertTrue(isVisible(module.$.files));
    assertEquals(3, items.length);
    assertEquals(
        'Bar',
        items[1]!.querySelector<HTMLElement>('.file-title')!.textContent);
    assertEquals(
        'Edited yesterday',
        items[1]!.querySelector<HTMLElement>('.file-description')!.textContent);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.spreadsheet',
        items[0]!.querySelector<CrAutoImgElement>('.file-icon')!.autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.document',
        items[1]!.querySelector<CrAutoImgElement>('.file-icon')!.autoSrc);
    assertEquals('https://foo.com/', urls[0]!.href);
    assertEquals('https://bar.com/', urls[1]!.href);
  });

  test('module does not render if there are no files', async () => {
    handler.setResultFor('getFiles', Promise.resolve({files: []}));

    const module = await driveV2Descriptor.initialize(0);
    await handler.whenCalled('getFiles');
    assertFalse(!!module);
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
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));
    const driveModule =
        await driveV2Descriptor.initialize(0) as DriveV2ModuleElement;
    assertTrue(!!driveModule);
    document.body.append(driveModule);

    // Act.
    ($$(driveModule, 'ntp-module-header-v2')!
     ).dispatchEvent(new Event('info-button-click'));

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
          ],
        };
        handler.setResultFor('getFiles', Promise.resolve(data));
        const driveModule =
            await driveV2Descriptor.initialize(0) as DriveV2ModuleElement;
        document.body.append(driveModule);

        // Act.
        const whenFired = eventToPromise('disable-module', driveModule);
        ($$(driveModule, 'ntp-module-header-v2')!
         ).dispatchEvent(new Event('disable-button-click'));

        // Assert.
        const event: DisableModuleEvent = await whenFired;
        assertEquals(
            'You won\'t see Drive files on this page again',
            event.detail.message);
      });
});
