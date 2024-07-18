// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileSuggestionHandlerRemote} from 'chrome://new-tab-page/file_suggestion.mojom-webui.js';
import type {DisableModuleEvent, DismissModuleEvent, FileSuggestionModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {FileProxy, fileSuggestionDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import type {CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

suite('FileSuggestionV2Module', () => {
  let handler: TestMock<FileSuggestionHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(FileSuggestionHandlerRemote, FileProxy.setHandler);
  });

  test('shows all retrieved files on render', async () => {
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
          id: '132',
          mimeType: 'application/vnd.google-apps.document',
          itemUrl: {url: 'https://bar.com'},
        },
        {
          justificationText: 'Created today',
          title: 'Baz',
          id: '213',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://baz.com'},
        },
        {
          justificationText: 'Created yesterday',
          title: 'Qux',
          id: '231',
          mimeType: 'application/vnd.google-apps.presentation',
          itemUrl: {url: 'https://qux.com'},
        },
        {
          justificationText: 'Edited last week',
          title: 'FooBar',
          id: '312',
          mimeType: 'application/vnd.google-apps.spreadsheet',
          itemUrl: {url: 'https://foo.com'},
        },
        {
          justificationText: 'Edited yesterday',
          title: 'BazQux',
          id: '321',
          mimeType: 'application/vnd.google-apps.document',
          itemUrl: {url: 'https://bar.com'},
        },
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module =
        await fileSuggestionDescriptor.initialize(0) as FileSuggestionModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getFiles');
    await microtasksFinished();
    const items = Array.from(module.shadowRoot!.querySelectorAll('.file'));

    assertTrue(isVisible(module.$.files));
    assertEquals(6, items.length);
  });

  test('parses file data correctly', async () => {
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
          id: '132',
          mimeType: 'application/vnd.google-apps.document',
          itemUrl: {url: 'https://bar.com'},
        },
      ],
    };
    handler.setResultFor('getFiles', Promise.resolve(data));

    const module =
        await fileSuggestionDescriptor.initialize(0) as FileSuggestionModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getFiles');
    await microtasksFinished();
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

  test('module does not render if there are no files', async () => {
    handler.setResultFor('getFiles', Promise.resolve({files: []}));

    const module = await fileSuggestionDescriptor.initialize(0);
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
        await fileSuggestionDescriptor.initialize(0) as FileSuggestionModuleElement;
    assertTrue(!!driveModule);
    document.body.append(driveModule);
    await microtasksFinished();

    // Act.
    const infoButton = driveModule.$.moduleHeaderElementV2.shadowRoot!
                           .querySelector<HTMLElement>('#info');
    assertTrue(!!infoButton);
    infoButton.click();
    await microtasksFinished();

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
            await fileSuggestionDescriptor.initialize(0) as FileSuggestionModuleElement;
        document.body.append(driveModule);
        await microtasksFinished();

        // Act.
        const whenFired = eventToPromise('disable-module', driveModule);
        const disableButton = driveModule.$.moduleHeaderElementV2.shadowRoot!
                                  .querySelector<HTMLElement>('#disable');
        assertTrue(!!disableButton);
        disableButton.click();

        // Assert.
        const event: DisableModuleEvent = await whenFired;
        assertEquals(
            'You won\'t see Drive files on this page again',
            event.detail.message);
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
        await fileSuggestionDescriptor.initialize(0) as FileSuggestionModuleElement;
    assertTrue(!!moduleElement);
    document.body.append(moduleElement);
    await microtasksFinished();

    // Act.
    const whenFired = eventToPromise('dismiss-module-instance', moduleElement);
    const dismissButton = moduleElement.$.moduleHeaderElementV2.shadowRoot!
                              .querySelector<HTMLElement>('#dismiss');
    assertTrue(!!dismissButton);
    dismissButton.click();

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
});
