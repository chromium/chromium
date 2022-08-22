// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/cloud_upload_dialog.js';

import {PageHandlerRemote, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudUploadElement} from 'chrome://cloud-upload/cloud_upload_dialog.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test CloudUploadBrowserProxy implementation that enables to mock various
 * mojo responses.
 */
class CloudUploadTestBrowserProxy implements CloudUploadBrowserProxy {
  handler: PageHandlerRemote&TestBrowserProxy;

  constructor() {
    this.handler = TestBrowserProxy.fromClass(PageHandlerRemote);
  }

  getDialogArguments() {
    return JSON.stringify({fileName: 'file.docx'});
  }
}

/**
 * Wait for `f` to evaluate to true. Evaluation interval is 100ms, throws an
 * error if the 5s timeout is reached.
 */
async function waitFor(f: () => boolean) {
  return new Promise((resolve, reject) => {
    const intervalId = setInterval(() => {
      if (f()) {
        clearInterval(intervalId);
        clearTimeout(timeoutId);
        resolve(undefined);
      }
    }, 100);

    const timeoutId = setTimeout(() => {
      clearInterval(intervalId);
      reject(new Error('waitFor has timed out'));
    }, 5000);
  });
}

suite('<cloud-upload>', () => {
  /* Holds the <cloud-upload> app. */
  let container: HTMLDivElement;
  /* The <cloud-upload> app. */
  let cloudUploadApp: CloudUploadElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testProxy: CloudUploadTestBrowserProxy;
  /* Upload path, response from the `getUploadPath` mojo method. */
  const uploadPath = '/from Chromebook';

  /**
   * Runs prior to all the tests running, attaches a div to enable isolated
   * clearing and attaching of the web component.
   */
  suiteSetup(() => {
    container = document.createElement('div');
    document.body.appendChild(container);
    testProxy = new CloudUploadTestBrowserProxy();
    CloudUploadBrowserProxy.setInstance(testProxy);
  });

  /**
   * Runs before each test.
   */
  setup(() => {
    // Sets `getUploadPath` to return the 'from Chromebook' static string.
    // Called on <cloud-upload>'s connectedCallback.
    testProxy.handler.setResultFor('getUploadPath', {
      uploadPath: {
        path: uploadPath,
      },
    });
    // Creates and attaches the <cloud-upload> element to the DOM tree.
    cloudUploadApp =
        document.createElement('cloud-upload') as CloudUploadElement;
    container.appendChild(cloudUploadApp);
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
   * Tests that the upload path is correctly displayed when the <cloud-upload>
   * element is attached.
   */
  test('Upload location', async () => {
    const uploadLocationElement = cloudUploadApp.$('#upload-location');
    // Wait for the #upload-location element (initially empty) to update.
    await waitFor(() => !!uploadLocationElement.innerText);
    assertEquals(
        `Upload location: ${uploadPath}`, uploadLocationElement.innerText);
  });

  /**
   * Tests that clicking the upload button triggers the right `respondAndClose`
   * mojo request.
   */
  test('Upload button', async () => {
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
    cloudUploadApp.$('#cancel-button').click();
    await testProxy.handler.whenCalled('respondAndClose');
    assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
    assertDeepEquals(
        [UserAction.kCancel], testProxy.handler.getArgs('respondAndClose'));
  });
});
