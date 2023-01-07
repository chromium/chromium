// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/cloud_upload_dialog.js';

import {PageHandlerRemote, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudUploadElement, UploadType} from 'chrome://cloud-upload/cloud_upload_dialog.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test CloudUploadBrowserProxy implementation that enables to mock various
 * mojo responses.
 */
class CloudUploadTestBrowserProxy implements CloudUploadBrowserProxy {
  handler: PageHandlerRemote&TestBrowserProxy;
  uploadType: UploadType;

  constructor(uploadType: UploadType) {
    this.handler = TestBrowserProxy.fromClass(PageHandlerRemote);
    this.uploadType = uploadType;
  }

  getDialogArguments() {
    switch (this.uploadType) {
      case UploadType.ONE_DRIVE:
        return JSON.stringify({fileName: 'file.docx', uploadType: 'OneDrive'});
      case UploadType.DRIVE:
        return JSON.stringify({fileName: 'file.docx', uploadType: 'Drive'});
      default:
        return JSON.stringify({fileName: 'file.docx', uploadType: ''});
    }
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

  const setupForUploadType = (uploadType: UploadType) => {
    testProxy = new CloudUploadTestBrowserProxy(uploadType);
    CloudUploadBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <cloud-upload> element to the DOM tree.
    cloudUploadApp =
        document.createElement('cloud-upload') as CloudUploadElement;
    container.appendChild(cloudUploadApp);
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
   * Tests that the dialog's content is correct for OneDrive.
   */
  test('Upload to OneDrive - title', async () => {
    setupForUploadType(UploadType.ONE_DRIVE);
    const titleElement = cloudUploadApp.$('div[slot="title"]');
    const uploadButton = cloudUploadApp.$('#upload-button');
    assertEquals(`Upload to OneDrive`, titleElement.innerText);
    assertEquals(`Upload to OneDrive`, uploadButton.innerText);
  });

  /**
   * Tests that the dialog's content is correct for Drive.
   */
  test('Upload to Drive - title', async () => {
    setupForUploadType(UploadType.DRIVE);
    const titleElement = cloudUploadApp.$('div[slot="title"]');
    const uploadButton = cloudUploadApp.$('#upload-button');
    assertEquals(`Upload to Drive`, titleElement.innerText);
    assertEquals(`Upload to Drive`, uploadButton.innerText);
  });

  /**
   * Tests that clicking the upload button triggers the right `respondAndClose`
   * mojo request.
   */
  test('Upload button', async () => {
    setupForUploadType(UploadType.DRIVE);
    cloudUploadApp.$('#upload-button').click();
    await testProxy.handler.whenCalled('respondAndClose');
    assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
    assertDeepEquals(
        [UserAction.kUpload], testProxy.handler.getArgs('respondAndClose'));
  });

  /**
   * Tests that clicking the cancel button triggers the right `respondAndClose`
   * mojo request.
   */
  test('Cancel button', async () => {
    setupForUploadType(UploadType.DRIVE);
    cloudUploadApp.$('#cancel-button').click();
    await testProxy.handler.whenCalled('respondAndClose');
    assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
    assertDeepEquals(
        [UserAction.kCancel], testProxy.handler.getArgs('respondAndClose'));
  });
});
