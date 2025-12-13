// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {File} from 'chrome://new-tab-page/file_suggestion.mojom-webui.js';
import {RecommendationType} from 'chrome://new-tab-page/file_suggestion.mojom-webui.js';
import type {DisableModuleEvent, DismissModuleInstanceEvent, MicrosoftFilesModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {microsoftFilesModuleDescriptor, MicrosoftFilesProxyImpl, ParentTrustedDocumentProxy} from 'chrome://new-tab-page/lazy_load.js';
import {MicrosoftFilesPageHandlerRemote} from 'chrome://new-tab-page/microsoft_files.mojom-webui.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {MicrosoftAuthUntrustedDocumentRemote} from 'chrome://new-tab-page/ntp_microsoft_auth_shared_ui.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('MicrosoftFilesModule', () => {
  let childDocument: TestMock<MicrosoftAuthUntrustedDocumentRemote>;
  let handler: TestMock<MicrosoftFilesPageHandlerRemote>;
  const modulesMicrosoftFilesName = 'SharePoint and OneDrive files';

  setup(() => {
    loadTimeData.overrideValues(
        {modulesMicrosoftFilesName: modulesMicrosoftFilesName});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        MicrosoftFilesPageHandlerRemote,
        mock => MicrosoftFilesProxyImpl.setInstance(
            new MicrosoftFilesProxyImpl(mock)));
    childDocument = installMock(
        MicrosoftAuthUntrustedDocumentRemote,
        mock => ParentTrustedDocumentProxy.setInstance(mock));
  });

  function createFiles(numFiles: number): File[] {
    const files: File[] = [];
    for (let i = 0; i < numFiles; i++) {
      files.push({
        justificationText: 'Trending in your organization',
        title: `Document ${i}`,
        id: `${i}`,
        iconUrl: {url: 'https://foo.com/'},
        itemUrl: {url: `https://foo.com/${i}`},
        recommendationType: RecommendationType.kUsed,
      });
    }
    return files;
  }

  test('clicking the info button opens the ntp info dialog box', async () => {
    // Arrange.
    handler.setResultFor('getFiles', Promise.resolve({files: createFiles(6)}));
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    assertTrue(!!microsoftFilesModule);
    document.body.append(microsoftFilesModule);
    await microtasksFinished();
    assertFalse(!!$$(microsoftFilesModule, 'ntp-info-dialog'));

    // Act.
    const infoButton = microsoftFilesModule.$.moduleHeaderElementV2.shadowRoot
                           .querySelector<HTMLElement>('#info');
    assertTrue(!!infoButton);
    infoButton.click();
    await microtasksFinished();

    // Assert.
    assertTrue(!!$$(microsoftFilesModule, 'ntp-info-dialog'));
  });

  test('clicking the disable button fires a disable module event', async () => {
    // Arrange.
    handler.setResultFor('getFiles', Promise.resolve({files: createFiles(6)}));
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    assertTrue(!!microsoftFilesModule);
    document.body.append(microsoftFilesModule);
    await microtasksFinished();

    // Act.
    const whenFired = eventToPromise('disable-module', microsoftFilesModule);
    const disableButton =
        microsoftFilesModule.$.moduleHeaderElementV2.shadowRoot
            .querySelector<HTMLElement>('#disable');
    assertTrue(!!disableButton);
    disableButton.click();

    // Assert.
    const event: DisableModuleEvent = await whenFired;
    assertEquals(
        ('You won\'t see ' + modulesMicrosoftFilesName + ' on this page again'),
        event.detail.message);
  });

  test('clicking the sign out button sends sign out request', async () => {
    // Arrange.
    handler.setResultFor('getFiles', Promise.resolve({files: createFiles(6)}));
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    assertTrue(!!microsoftFilesModule);
    document.body.append(microsoftFilesModule);
    await microtasksFinished();

    // Act.
    const signoutButton =
        microsoftFilesModule.$.moduleHeaderElementV2.shadowRoot
            .querySelector<HTMLElement>('#signout');
    assertTrue(!!signoutButton);
    signoutButton.click();

    // Assert.
    assertEquals(1, childDocument.getCallCount('signOut'));
  });

  test('creates module', async () => {
    // Set up module.
    handler.setResultFor('getFiles', Promise.resolve({files: createFiles(6)}));
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    assertTrue(!!microsoftFilesModule);
    document.body.append(microsoftFilesModule);
    await microtasksFinished();

    // Assert.
    assertTrue(isVisible(microsoftFilesModule.$.moduleHeaderElementV2));
    assertEquals(
        microsoftFilesModule.$.moduleHeaderElementV2.headerText,
        modulesMicrosoftFilesName);
  });

  test('module not created when there are no files', async () => {
    handler.setResultFor('getFiles', Promise.resolve({files: createFiles(0)}));
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;

    assertEquals(microsoftFilesModule, null);
  });

  test('dismiss and restore module', async () => {
    // Set up module.
    handler.setResultFor('getFiles', Promise.resolve({files: createFiles(3)}));
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    assertTrue(!!microsoftFilesModule);
    document.body.append(microsoftFilesModule);
    await microtasksFinished();

    // Dismiss module.
    const whenFired =
        eventToPromise('dismiss-module-instance', microsoftFilesModule);
    const dismissButton =
        microsoftFilesModule.$.moduleHeaderElementV2.shadowRoot
            .querySelector<HTMLElement>('#dismiss');
    assertTrue(!!dismissButton);
    dismissButton.click();

    const event: DismissModuleInstanceEvent = await whenFired;
    assertEquals('Files hidden', event.detail.message);
    assertTrue(!!event.detail.restoreCallback);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Restore module.
    event.detail.restoreCallback();
    assertEquals(1, handler.getCallCount('restoreModule'));
  });

  test('file recommendation type counts are logged', async () => {
    const metrics = fakeMetricsPrivate();
    // Set up module.
    const numFiles = 3;
    handler.setResultFor(
        'getFiles', Promise.resolve({files: createFiles(numFiles)}));
    const microsoftFilesModule =
        await microsoftFilesModuleDescriptor.initialize(0) as
        MicrosoftFilesModuleElement;
    assertTrue(!!microsoftFilesModule);
    document.body.append(microsoftFilesModule);
    await microtasksFinished();

    assertEquals(
        1,
        metrics.count('NewTabPage.MicrosoftFiles.ShownFiles.Used', numFiles));
    assertEquals(
        1, metrics.count('NewTabPage.MicrosoftFiles.ShownFiles.Trending', 0));
    assertEquals(
        1, metrics.count('NewTabPage.MicrosoftFiles.ShownFiles.Shared', 0));
  });
});
