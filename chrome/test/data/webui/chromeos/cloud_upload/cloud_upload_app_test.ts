// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/cloud_upload_dialog.js';

import {DialogArgs, DialogPage, PageHandlerRemote, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudUploadElement} from 'chrome://cloud-upload/cloud_upload_dialog.js';
import {OfficePwaInstallPageElement} from 'chrome://cloud-upload/office_pwa_install_page.js';
import {OneDriveUploadPageElement} from 'chrome://cloud-upload/one_drive_upload_page.js';
import {SetupCancelDialogElement} from 'chrome://cloud-upload/setup_cancel_dialog.js';
import {SignInPageElement} from 'chrome://cloud-upload/sign_in_page.js';
import {WelcomePageElement} from 'chrome://cloud-upload/welcome_page.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

interface ProxyOptions {
  fileName?: string|null;
  officeWebAppInstalled: boolean;
  installOfficeWebAppResult: boolean;
  odfsMounted: boolean;
}

/**
 * A test CloudUploadBrowserProxy implementation that enables to mock various
 * mojo responses.
 */
class CloudUploadTestBrowserProxy implements CloudUploadBrowserProxy {
  handler: PageHandlerRemote&TestBrowserProxy;

  constructor(options: ProxyOptions) {
    this.handler = TestBrowserProxy.fromClass(PageHandlerRemote);
    const args: DialogArgs = {
      fileNames: [],
      dialogPage: DialogPage.kOneDriveSetup,
    };
    if (options.fileName != null) {
      args.fileNames.push(options.fileName);
    }
    this.handler.setResultFor('getDialogArgs', {args: args});
    this.handler.setResultFor(
        'isOfficeWebAppInstalled', {installed: options.officeWebAppInstalled});
    this.handler.setResultFor(
        'installOfficeWebApp', {installed: options.installOfficeWebAppResult});
    this.handler.setResultFor('isODFSMounted', {mounted: options.odfsMounted});
    this.handler.setResultFor('signInToOneDrive', {success: true});
  }

  isTest() {
    return true;
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

  async function setUp(options: ProxyOptions) {
    testProxy = new CloudUploadTestBrowserProxy(options);
    CloudUploadBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <cloud-upload> element to the DOM tree.
    cloudUploadApp =
        document.createElement('cloud-upload') as CloudUploadElement;
    container.appendChild(cloudUploadApp);
    await cloudUploadApp.initPromise;
  }

  function checkIsWelcomePage(): void {
    assertTrue(cloudUploadApp.currentPage instanceof WelcomePageElement);
  }

  function checkIsInstallPage(): void {
    assertTrue(
        cloudUploadApp.currentPage instanceof OfficePwaInstallPageElement);
  }

  function checkIsSignInPage(): void {
    assertTrue(cloudUploadApp.currentPage instanceof SignInPageElement);
  }

  function checkIsOneDriveUploadPage(): void {
    assertTrue(cloudUploadApp.currentPage instanceof OneDriveUploadPageElement);
  }

  async function waitForNextPage(): Promise<void> {
    // This promise resolves once a new page appears.
    return new Promise<void>(resolve => {
      const observer = new MutationObserver(mutations => {
        for (const mutation of mutations) {
          if (mutation.addedNodes.length > 0) {
            observer.disconnect();
            resolve();
          }
        }
      });
      observer.observe(cloudUploadApp.shadowRoot!, {childList: true});
    });
  }

  async function doWelcomePage(): Promise<void> {
    checkIsWelcomePage();
    const nextPagePromise = waitForNextPage();
    cloudUploadApp.$('.action-button').click();
    await nextPagePromise;
  }

  async function doPWAInstallPage(): Promise<void> {
    checkIsInstallPage();
    const nextPagePromise = waitForNextPage();
    cloudUploadApp.$('.action-button').click();
    await nextPagePromise;
  }

  async function doSignInPage(): Promise<void> {
    checkIsSignInPage();
    const nextPagePromise = waitForNextPage();
    cloudUploadApp.$('.action-button').click();
    await nextPagePromise;
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
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: true,
      odfsMounted: false,
    });

    // Go to the OneDrive upload page.
    await doWelcomePage();
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
    const fileContainer = cloudUploadApp.$('#file-container');
    assertFalse(fileContainer.hidden);
  });

  /**
   * Tests that the dialog's content is correct for OneDrive when there is no
   * file.
   */
  test('Set up OneDrive without file', async () => {
    await setUp({
      officeWebAppInstalled: false,
      installOfficeWebAppResult: true,
      odfsMounted: false,
    });

    // Go to the OneDrive upload page.
    await doWelcomePage();
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
    const fileContainer = cloudUploadApp.$('#file-container');
    assertTrue(fileContainer.hidden);
  });

  /**
   * Tests that there is no PWA install page when the Office PWA is already
   * installed.
   */
  test('Set up OneDrive with Office PWA already installed', async () => {
    await setUp({
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: false,
    });

    await doWelcomePage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
  });

  /**
   * Tests that there is no sign in page when ODFS is already mounted.
   */
  test('Set up OneDrive with user already signed in', async () => {
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: true,
      odfsMounted: true,
    });

    await doWelcomePage();
    await doPWAInstallPage();

    checkIsOneDriveUploadPage();
  });

  /**
   * Tests that there is no Office PWA install page or sign in page when the
   * Office PWA is already installed and ODFS is already mounted.
   */
  test(
      'Set up OneDrive with Office PWA already installed and already signed in',
      async () => {
        await setUp({
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
        });

        await doWelcomePage();

        checkIsOneDriveUploadPage();
      });

  /**
   * Tests that clicking the open file button triggers the right
   * `respondAndClose` mojo request.
   */
  test('Open file button', async () => {
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: true,
      odfsMounted: false,
    });
    checkIsWelcomePage();

    // Click the 'next' button on the welcome page.
    cloudUploadApp.$('.action-button').click();

    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
    cloudUploadApp.$('.action-button').click();
    await testProxy.handler.whenCalled('respondAndClose');
    assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
    assertDeepEquals(
        [UserAction.kConfirmOrUploadToOneDrive],
        testProxy.handler.getArgs('respondAndClose'));
  });

  /**
   * Tests that clicking the close button on the last page triggers the right
   * `respondAndClose` mojo request.
   */
  test('Close button on last page', async () => {
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: true,
      odfsMounted: false,
    });

    // Go to the OneDrive upload page.
    await doWelcomePage();
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
    cloudUploadApp.$('.cancel-button').click();
    await testProxy.handler.whenCalled('respondAndClose');
    assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
    assertDeepEquals(
        [UserAction.kCancel], testProxy.handler.getArgs('respondAndClose'));
  });

  /**
   * Tests that the cancel button should show the cancel dialog on each page
   * except the last page.
   */
  [1, 2, 3].forEach(
      page => test(`Close button on page ${page}`, async () => {
        await setUp({
          officeWebAppInstalled: false,
          installOfficeWebAppResult: true,
          odfsMounted: false,
        });

        // Go to the specified page.
        if (page > 1) {
          await doWelcomePage();
        }
        if (page > 2) {
          await doPWAInstallPage();
        }

        // Bring up the cancel dialog and dismiss it.
        cloudUploadApp.$('.cancel-button').click();
        const cancelDialog =
            cloudUploadApp.$<SetupCancelDialogElement>('setup-cancel-dialog')!;
        assertTrue(cancelDialog.open);
        cancelDialog.$('.action-button').click();
        assertFalse(cancelDialog.open);

        // Bring up the cancel dialog and cancel setup.
        cloudUploadApp.$('.cancel-button').click();
        assertTrue(cancelDialog.open);
        assertEquals(0, testProxy.handler.getCallCount('respondAndClose'));

        cancelDialog.$('.cancel-button').click();
        await testProxy.handler.whenCalled('respondAndClose');
        assertEquals(1, testProxy.handler.getCallCount('respondAndClose'));
        assertDeepEquals(
            [UserAction.kCancel], testProxy.handler.getArgs('respondAndClose'));
      }));
});
