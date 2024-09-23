// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileSuggestionHandlerRemote} from 'chrome://new-tab-page/file_suggestion.mojom-webui.js';
import type {DismissModuleEvent, DriveModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {driveDescriptor, FileProxy} from 'chrome://new-tab-page/lazy_load.js';
import type {CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesDriveModuleTest', () => {
  let handler: TestMock<FileSuggestionHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(FileSuggestionHandlerRemote, FileProxy.setHandler);
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
        },
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module = await driveDescriptor.initialize(0) as DriveModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getFiles');
    module.$.fileRepeat.render();

    const items =
        module.shadowRoot!.querySelectorAll<HTMLAnchorElement>('.file');
    assertTrue(!!module);
    assertTrue(isVisible(module.$.files));
    assertEquals(3, items.length);
    assertEquals('Bar', items[1]!.querySelector('.file-title')!.textContent);
    assertEquals(
        'Edited today',
        items[1]!.querySelector('.file-description')!.textContent);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.spreadsheet',
        items[0]!.querySelector<CrAutoImgElement>('.file-icon')!.autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.document',
        items[1]!.querySelector<CrAutoImgElement>('.file-icon')!.autoSrc);
    assertEquals(
        'https://drive-thirdparty.googleusercontent.com/32/type/application/vnd.google-apps.presentation',
        items[2]!.querySelector<CrAutoImgElement>('.file-icon')!.autoSrc);
    assertEquals('https://foo.com/', items[0]!.href);
    assertEquals('https://bar.com/', items[1]!.href);
    assertEquals('https://caz.com/', items[2]!.href);
  });

  test('documents do not show without data', async () => {
    handler.setResultFor('getFiles', Promise.resolve({files: []}));

    const module = await driveDescriptor.initialize(0);
    await handler.whenCalled('getFiles');
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
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));
    const moduleElement =
        await driveDescriptor.initialize(0) as DriveModuleElement;
    assertTrue(!!moduleElement);
    document.body.append(moduleElement);

    // Act.
    const whenFired = eventToPromise('dismiss-module', moduleElement);
    ($$(moduleElement, 'ntp-module-header')!
     ).dispatchEvent(new Event('dismiss-button-click'));

    // Assert.
    const event: DismissModuleEvent = await whenFired;
    assertEquals('Files hidden', event.detail.message);
    assertTrue(!!event.detail.restoreCallback);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    event.detail.restoreCallback!();

    // Assert.
    assertEquals(1, handler.getCallCount('restoreModule'));
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
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));
    const module = await driveDescriptor.initialize(0) as DriveModuleElement;
    assertTrue(!!module);
    document.body.append(module);

    // Act.
    ($$(module, 'ntp-module-header')!
     ).dispatchEvent(new Event('info-button-click'));

    // Assert.
    assertTrue(!!$$(module, 'ntp-info-dialog'));
  });
});
