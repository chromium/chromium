// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {DriveHandlerRemote} from 'chrome://new-tab-page/drive.mojom-webui.js';
import {DisableModuleEvent, DriveProxy, driveV2Descriptor, DriveV2ModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$, CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesDriveModuleTest', () => {
  let handler: TestBrowserProxy<DriveHandlerRemote>;

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
          title: 'Caz',
          id: '345',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://caz.com'},
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
    assertEquals(2, items.length);
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
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module =
        await driveV2Descriptor.initialize(0) as DriveV2ModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getFiles');
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
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module =
        await driveV2Descriptor.initialize(0) as DriveV2ModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getFiles');
    module.$.fileRepeat.render();

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
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));
    const driveModule =
        await driveV2Descriptor.initialize(0) as DriveV2ModuleElement;
    assertTrue(!!driveModule);
    document.body.append(driveModule);

    // Act.
    ($$(driveModule, 'ntp-module-header')!
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
        ($$(driveModule, 'ntp-module-header')!
         ).dispatchEvent(new Event('disable-button-click'));

        // Assert.
        const event: DisableModuleEvent = await whenFired;
        assertEquals(
            'You won\'t see Drive files again on this page',
            event.detail.message);
      });
});
