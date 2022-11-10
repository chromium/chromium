// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/cloud_upload_dialog.js';

import {CloudProvider, DialogArgs, PageHandlerRemote, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudUploadElement} from 'chrome://cloud-upload/cloud_upload_dialog.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test CloudUploadBrowserProxy implementation that enables to mock various
 * mojo responses.
 */
class CloudUploadTestBrowserProxy implements CloudUploadBrowserProxy {
  handler: PageHandlerRemote&TestBrowserProxy;

  constructor(uploadType: CloudProvider, fileName: string|null) {
    this.handler = TestBrowserProxy.fromClass(PageHandlerRemote);
    const args: DialogArgs = {
      cloudProvider: uploadType,
      fileNames: [],
    };
    if (fileName != null) {
      args.fileNames.push(fileName);
    }
    this.handler.setResultFor('getDialogArgs', {args: args});
  }
}

suite('<cloud-upload>', () => {
  /* Holds the <cloud-upload> app. */
  let container: HTMLDivElement;
  /* The <cloud-upload> app. */
  let cloudUploadApp: CloudUploadElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testProxy: CloudUploadTestBrowserProxy;

  const setupForUploadType =
      async (uploadType: CloudProvider, fileName: string|null) => {
    testProxy = new CloudUploadTestBrowserProxy(uploadType, fileName);
    CloudUploadBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <cloud-upload> element to the DOM tree.
    cloudUploadApp =
        document.createElement('cloud-upload') as CloudUploadElement;
    container.appendChild(cloudUploadApp);
    await cloudUploadApp.initPromise;

    // Click the 'next' button on the welcome page.
    cloudUploadApp.$('.action-button').click();
  };

  /**
   * Runs prior to all the tests running, attaches a div to enable isolated
   * clearing and attaching of the web component.
   */
  suiteSetup(() => {
    container = document.createElement('div');
    document.body.appendChild(container);
  });

  /**
   * Runs after each test. Removes all elements from the <div> that holds
   * the <cloud-upload> component.
   */
  teardown(() => {
    container.innerHTML = '';
    testProxy.handler.reset();
  });

  /**
   * Tests that the dialog's content is correct for OneDrive when there is a
   * file.
   */
  test('Set up OneDrive with file', async () => {
    await setupForUploadType(CloudProvider.kOneDrive, 'file.docx');
    const fileContainer = cloudUploadApp.$('#file-container');
    assertFalse(fileContainer.hidden);
  });

  /**
   * Tests that the dialog's content is correct for OneDrive when there is no
   * file.
   */
  test('Set up OneDrive without file', async () => {
    await setupForUploadType(CloudProvider.kOneDrive, null);
    const fileContainer = cloudUploadApp.$('#file-container');
    assertTrue(fileContainer.hidden);
  });

  /**
   * Tests that clicking the open file button triggers the right
   * `respondAndClose` mojo request.
   */
  test('Open file button', async () => {
    await setupForUploadType(CloudProvider.kGoogleDrive, 'file.docx');
    cloudUploadApp.$('.action-button').click();
    await testProxy.handler.whenCalled('respondAndClose');
    assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
    assertDeepEquals(
        [UserAction.kUpload], testProxy.handler.getArgs('respondAndClose'));
  });

  /**
   * Tests that clicking the close button triggers the right `respondAndClose`
   * mojo request.
   */
  test('Close button', async () => {
    await setupForUploadType(CloudProvider.kGoogleDrive, 'file.docx');
    cloudUploadApp.$('.cancel-button').click();
    await testProxy.handler.whenCalled('respondAndClose');
    assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
    assertDeepEquals(
        [UserAction.kCancel], testProxy.handler.getArgs('respondAndClose'));
  });
});
