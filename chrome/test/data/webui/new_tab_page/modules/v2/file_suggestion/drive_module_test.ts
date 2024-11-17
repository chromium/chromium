// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DriveSuggestionHandlerRemote} from 'chrome://new-tab-page/drive_suggestion.mojom-webui.js';
import type {DisableModuleEvent, DismissModuleInstanceEvent, DriveModuleV2Element} from 'chrome://new-tab-page/lazy_load.js';
import {driveModuleV2Descriptor, FileProxy} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

suite('DriveModuleV2', () => {
  let handler: TestMock<DriveSuggestionHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(DriveSuggestionHandlerRemote, FileProxy.setHandler);
  });

  test(
      'setting files via handler populates `ntp-file-suggestion`', async () => {
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
            await driveModuleV2Descriptor.initialize(0) as DriveModuleV2Element;
        assertTrue(!!module);
        document.body.append(module);
        await handler.whenCalled('getFiles');
        await microtasksFinished();
        const fileSuggestion = module.$.fileSuggestion;
        const items =
            Array.from(fileSuggestion.shadowRoot!.querySelectorAll('.file'));

        assertEquals(6, items.length);
      });

  test('module does not render if there are no files', async () => {
    handler.setResultFor('getFiles', Promise.resolve({files: []}));

    const module = await driveModuleV2Descriptor.initialize(0);
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
        await driveModuleV2Descriptor.initialize(0) as DriveModuleV2Element;
    assertTrue(!!driveModule);
    document.body.append(driveModule);
    await microtasksFinished();
    assertFalse(!!$$(driveModule, 'ntp-info-dialog'));

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
            await driveModuleV2Descriptor.initialize(0) as DriveModuleV2Element;
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
            'You won\'t see Google Drive on this page again',
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
        await driveModuleV2Descriptor.initialize(0) as DriveModuleV2Element;
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
    const event: DismissModuleInstanceEvent = await whenFired;
    assertEquals('Files hidden', event.detail.message);
    assertTrue(!!event.detail.restoreCallback);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    event.detail.restoreCallback!();

    // Assert.
    assertEquals(1, handler.getCallCount('restoreModule'));
  });

  test('clicking file records correct metrics', async () => {
    const metrics = fakeMetricsPrivate();

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
        await driveModuleV2Descriptor.initialize(0) as DriveModuleV2Element;
    assertTrue(!!driveModule);
    document.body.append(driveModule);
    await microtasksFinished();

    // Act.
    const fileSuggestion = driveModule.$.fileSuggestion;
    const file = $$<HTMLElement>(fileSuggestion, '.file');
    assertTrue(!!file);
    file.click();
    await microtasksFinished();

    // Assert.
    assertEquals(1, metrics.count('NewTabPage.Drive.FileClick'));
    assertEquals(1, metrics.count('NewTabPage.Drive.FileClick', 0));
  });
});
