// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/connect_onedrive.js';

import {UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {ConnectOneDriveElement} from 'chrome://cloud-upload/connect_onedrive.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {CloudUploadTestBrowserProxy, ProxyOptions} from './cloud_upload_test_browser_proxy.js';

suite('<connect-onedrive>', () => {
  /* Holds the <connect-onedrive> app. */
  let container: HTMLDivElement;
  /* The <connect-onedrive> app. */
  let connectOneDriveApp: ConnectOneDriveElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testProxy: CloudUploadTestBrowserProxy;

  async function setUp(options: ProxyOptions) {
    testProxy = new CloudUploadTestBrowserProxy(options);
    CloudUploadBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <connect-onedrive> element to the DOM
    // tree.
    connectOneDriveApp =
        document.createElement('connect-onedrive') as ConnectOneDriveElement;
    container.appendChild(connectOneDriveApp);
  }

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
   * the <connect-onedrive> component.
   */
  teardown(() => {
    assert(window.trustedTypes);
    container.innerHTML = window.trustedTypes.emptyHTML;
    testProxy.handler.reset();
  });

  test('Successful connection leads to finished page', async () => {
    await setUp({
      fileNames: [],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        connectToOneDriveDialogArgs: {},
      },
    });

    const svgSuccess = connectOneDriveApp.$('#success')!;
    const title = connectOneDriveApp.$('#title')!;
    const bodyText = connectOneDriveApp.$('#body-text')!;

    assertEquals(svgSuccess.getAttribute('visibility'), 'hidden');
    assertTrue(title.innerText.includes('Connect to'), title.innerText);
    assertTrue(
        bodyText.innerText.includes('Connect OneDrive to'), bodyText.innerText);

    connectOneDriveApp.$('.action-button').click();

    await testProxy.handler.whenCalled('signInToOneDrive');
    assertEquals(svgSuccess.getAttribute('visibility'), 'visible');
    assertTrue(title.innerText.includes('connected'), title.innerText);
    assertTrue(
        bodyText.innerText.includes('OneDrive will now'), bodyText.innerText);
  });

  test('Failed connection leads to error page', async () => {
    await setUp({
      fileNames: [],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        connectToOneDriveDialogArgs: {},
      },
    });

    testProxy.handler.setResultFor('signInToOneDrive', {success: false});

    const errorMessage = connectOneDriveApp.$('#error-message')!;
    assertTrue(errorMessage.hasAttribute('hidden'));

    connectOneDriveApp.$('.action-button').click();

    await testProxy.handler.whenCalled('signInToOneDrive');
    assertFalse(errorMessage.hasAttribute('hidden'));

    // Try again but still fail.
    connectOneDriveApp.$('.action-button').click();

    await testProxy.handler.whenCalled('signInToOneDrive');
    assertFalse(errorMessage.hasAttribute('hidden'));
  });

  /**
   * Test that clicking the cancel button triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Cancel', async () => {
    await setUp({
      fileNames: [],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        connectToOneDriveDialogArgs: {},
      },
    });


    connectOneDriveApp.$('.cancel-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kCancel],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });

  /**
   * Test that an Escape keydown triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Escape', async () => {
    await setUp({
      fileNames: [],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        connectToOneDriveDialogArgs: {},
      },
    });


    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kCancel],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });
});
