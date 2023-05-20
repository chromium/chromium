// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/cloud_upload_dialog.js';

import {DialogPage, OperationType, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudUploadElement} from 'chrome://cloud-upload/cloud_upload_dialog.js';
import {OfficePwaInstallPageElement} from 'chrome://cloud-upload/office_pwa_install_page.js';
import {OneDriveUploadPageElement} from 'chrome://cloud-upload/one_drive_upload_page.js';
import {SetupCancelDialogElement} from 'chrome://cloud-upload/setup_cancel_dialog.js';
import {SignInPageElement} from 'chrome://cloud-upload/sign_in_page.js';
import {WelcomePageElement} from 'chrome://cloud-upload/welcome_page.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {CloudUploadTestBrowserProxy, ProxyOptions} from './cloud_upload_test_browser_proxy.js';

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

  function checkWelcomePage(
      officeWebAppInstalled: boolean, odfsMounted: boolean): void {
    assertTrue(cloudUploadApp.currentPage instanceof WelcomePageElement);

    // Check WelcomePage UI.
    assertEquals(
        !officeWebAppInstalled,
        cloudUploadApp.$('#description')
            .innerText.includes('Install Microsoft 365'));
    assertEquals(
        !odfsMounted,
        cloudUploadApp.$('#description')
            .innerText.includes('Connect to Microsoft OneDrive'));
    const zeroStep = officeWebAppInstalled && odfsMounted;
    const moveFilesDescription =
        'Files will move to OneDrive when opening in Microsoft 365';
    if (zeroStep) {
      assertTrue(cloudUploadApp.$('#description')
                     .innerText.includes(moveFilesDescription));
      assertEquals(cloudUploadApp.$('.action-button').innerText, 'Set up');
    } else {
      assertTrue(cloudUploadApp.$('#description')
                     .innerText.includes(moveFilesDescription));
      assertEquals(cloudUploadApp.$('.action-button').innerText, 'Get started');
    }
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

  async function doWelcomePage(
      officeWebAppInstalled: boolean, odfsMounted: boolean): Promise<void> {
    checkWelcomePage(officeWebAppInstalled, odfsMounted);

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
    assert(window.trustedTypes);
    container.innerHTML = window.trustedTypes.emptyHTML;
    testProxy.handler.reset();
  });

  /**
   * Tests that the dialog's content is correct for OneDrive when there is a
   * file.
   */
  test('Set up OneDrive with file', async () => {
    const officeWebAppInstalled = false;
    const odfsMounted = false;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogPage: DialogPage.kOneDriveSetup,
      operationType: OperationType.kMove,
    });

    // Go to the OneDrive upload page.
    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
  });

  /**
   * Tests that the dialog's content is correct for OneDrive when there is no
   * file.
   */
  test('Set up OneDrive without file', async () => {
    const officeWebAppInstalled = false;
    const odfsMounted = false;
    await setUp({
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogPage: DialogPage.kOneDriveSetup,
      operationType: OperationType.kMove,
    });

    // Go to the OneDrive upload page.
    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
  });

  /**
   * Tests that there is no PWA install page when the Office PWA is already
   * installed.
   */
  test('Set up OneDrive with Office PWA already installed', async () => {
    const officeWebAppInstalled = true;
    const odfsMounted = false;
    await setUp({
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogPage: DialogPage.kOneDriveSetup,
      operationType: OperationType.kMove,
    });

    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doSignInPage();

    checkIsOneDriveUploadPage();
  });

  /**
   * Tests that there is no sign in page when ODFS is already mounted.
   */
  test('Set up OneDrive with user already signed in', async () => {
    const officeWebAppInstalled = false;
    const odfsMounted = true;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogPage: DialogPage.kOneDriveSetup,
      operationType: OperationType.kMove,
    });

    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doPWAInstallPage();

    checkIsOneDriveUploadPage();
  });

  /**
   * Tests that when the Office PWA is already installed and ODFS is already
   * mounted, the welcome page shows the 0-step UI and there is no Office PWA
   * install page or sign in page.
   */
  test(
      'Set up OneDrive with Office PWA already installed and already signed in',
      async () => {
        const officeWebAppInstalled = true;
        const odfsMounted = true;
        await setUp({
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogPage: DialogPage.kOneDriveSetup,
          operationType: OperationType.kMove,
        });

        await doWelcomePage(officeWebAppInstalled, odfsMounted);

        checkIsOneDriveUploadPage();
      });

  /**
   * Tests that clicking the done button triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Open file button', async () => {
    const officeWebAppInstalled = false;
    const odfsMounted = false;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogPage: DialogPage.kOneDriveSetup,
      operationType: OperationType.kMove,
    });
    await doWelcomePage(officeWebAppInstalled, odfsMounted);

    // Click the 'next' button on the welcome page.
    cloudUploadApp.$('.action-button').click();

    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
    cloudUploadApp.$('.action-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kConfirmOrUploadToOneDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });

  /**
   * Tests that the cancel button should show the cancel dialog on each page
   * except the last page.
   */
  [1, 2, 3].forEach(
      page => test(`Close button on page ${page}`, async () => {
        const officeWebAppInstalled = false;
        const odfsMounted = false;
        await setUp({
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogPage: DialogPage.kOneDriveSetup,
          operationType: OperationType.kMove,
        });

        // Go to the specified page.
        if (page > 1) {
          await doWelcomePage(officeWebAppInstalled, odfsMounted);
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
        assertEquals(
            0, testProxy.handler.getCallCount('respondWithUserActionAndClose'));

        cancelDialog.$('.cancel-button').click();
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
        assertDeepEquals(
            [UserAction.kCancel],
            testProxy.handler.getArgs('respondWithUserActionAndClose'));
      }));

  /**
   * Test that completing the Setup flow from the OneDrive Upload Page triggers
   * the `setOfficeAsDefaultHandler` mojo request when this is the first time
   * the Setup flow is running.
   */
  test(
      'Office PWA set as default handler when the Setup flow is running',
      async () => {
        const officeWebAppInstalled = false;
        const odfsMounted = true;
        await setUp({
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogPage: DialogPage.kOneDriveSetup,
          firstTimeSetup: true,
          operationType: OperationType.kMove,
        });
        // Go to the OneDrive upload page.
        await doWelcomePage(officeWebAppInstalled, odfsMounted);
        await doPWAInstallPage();
        checkIsOneDriveUploadPage();

        // Click the open file button.
        cloudUploadApp.$('.action-button').click();

        assertEquals(
            1, testProxy.handler.getCallCount('setOfficeAsDefaultHandler'));
      });

  /**
   * Test that completing the Setup flow from the OneDrive Upload Page does not
   * trigger the `setOfficeAsDefaultHandler` mojo request when this is not the
   * first time the Setup flow is not running, i.e. the Fixup flow is running.
   */
  test(
      'Office PWA not set as default handler when the Fixup flow is running',
      async () => {
        const officeWebAppInstalled = false;
        const odfsMounted = true;
        await setUp({
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogPage: DialogPage.kOneDriveSetup,
          firstTimeSetup: false,
          operationType: OperationType.kMove,
        });
        // Go to the OneDrive upload page.
        await doWelcomePage(officeWebAppInstalled, odfsMounted);
        await doPWAInstallPage();
        checkIsOneDriveUploadPage();

        // Click the open file button.
        cloudUploadApp.$('.action-button').click();

        assertEquals(
            0, testProxy.handler.getCallCount('setOfficeAsDefaultHandler'));
      });
});
