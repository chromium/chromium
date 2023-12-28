// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/cloud_upload_dialog.js';

import {UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudUploadElement} from 'chrome://cloud-upload/cloud_upload_dialog.js';
import {OfficePwaInstallPageElement} from 'chrome://cloud-upload/office_pwa_install_page.js';
import {OfficeSetupCompletePageElement} from 'chrome://cloud-upload/office_setup_complete_page.js';
import {SetupCancelDialogElement} from 'chrome://cloud-upload/setup_cancel_dialog.js';
import {SignInPageElement} from 'chrome://cloud-upload/sign_in_page.js';
import {WelcomePageElement} from 'chrome://cloud-upload/welcome_page.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {whenCheck} from 'chrome://webui-test/test_util.js';

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

    // Setup fake strings.
    loadTimeData.resetForTesting({
      'cancel': 'Cancel',
      'close': 'Close',
      'open': 'Open',
      'install': 'Install',
      'installing': 'Installing',
      'installed': 'Installed',
      'connectToOneDriveTitle': 'Connect to OneDrive',
      'connectToOneDriveSignInFlowBodyText':
          'Connect to OneDrive body in Sign in',
      'connectToOneDriveBodyText': 'Connect to OneDrive body',
      'cantConnectOneDrive': 'Can not connect',
      'connectOneDrive': 'Connect',
      'oneDriveConnectedTitle': 'Connected',
      'oneDriveConnectedBodyText': 'Connected Body',
      'animationPlayText': 'Play',
      'animationPauseText': 'Pause',
      'fileHandlerTitle': 'Test title',
      'word': 'Word',
      'excel': 'Excel',
      'powerPoint': 'PowerPoint',
      'googleDocs': 'Google Docs',
      'googleSheets': 'Google Sheets',
      'googleSlides': 'Google Slides',
      'microsoft365': 'Microsoft 365',
      'otherApps': 'Other apps',
      'googleDriveStorage': 'Google Drive as storage',
      'oneDriveStorage': 'OneDrive as storage',
      'installPWATitle': 'Install PWA title',
      'installPWABodyText': 'Install PWA body',
      'welcomeBodyText': 'Welcome text for test. &lt;a href="#"&gt;&lt;/a&gt;',
      'welcomeGetStarted': 'Get started',
      'welcomeInstallOdfs': 'Connect to Microsoft OneDrive',
      'welcomeInstallOfficeWebApp': 'Install Microsoft 365',
      'welcomeMoveFiles':
          'Files will move to OneDrive when opening in Microsoft 365',
      'welcomeSetUp': 'Set up',
      'welcomeTitle': 'Set up Microsoft 365 to open files',
    });

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

  function checkIsOfficeSetupCompletePage(): void {
    assertTrue(
        cloudUploadApp.currentPage instanceof OfficeSetupCompletePageElement);
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
    loadTimeData.resetForTesting();
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
      fileNames: ['file.docx'],
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogSpecificArgs: {
        oneDriveSetupDialogArgs: {
          setOfficeAsDefaultHandler: true,
        },
      },
    });

    // Go to the OneDrive upload page.
    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOfficeSetupCompletePage();
  });

  /**
   * Tests that the dialog's content is correct for OneDrive when there is no
   * file.
   */
  test('Set up OneDrive without file', async () => {
    const officeWebAppInstalled = false;
    const odfsMounted = false;
    await setUp({
      fileNames: [],
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogSpecificArgs: {
        oneDriveSetupDialogArgs: {
          setOfficeAsDefaultHandler: true,
        },
      },
    });

    // Go to the OneDrive upload page.
    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOfficeSetupCompletePage();
  });

  /**
   * Tests that there is no PWA install page when the Office PWA is already
   * installed.
   */
  test('Set up OneDrive with Office PWA already installed', async () => {
    const officeWebAppInstalled = true;
    const odfsMounted = false;
    await setUp({
      fileNames: ['file.docx'],
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogSpecificArgs: {
        oneDriveSetupDialogArgs: {
          setOfficeAsDefaultHandler: true,
        },
      },
    });

    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doSignInPage();

    checkIsOfficeSetupCompletePage();
  });

  /**
   * Tests that there is no sign in page when ODFS is already mounted.
   */
  test('Set up OneDrive with user already signed in', async () => {
    const officeWebAppInstalled = false;
    const odfsMounted = true;
    await setUp({
      fileNames: ['file.docx'],
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogSpecificArgs: {
        oneDriveSetupDialogArgs: {
          setOfficeAsDefaultHandler: true,
        },
      },
    });

    await doWelcomePage(officeWebAppInstalled, odfsMounted);
    await doPWAInstallPage();

    checkIsOfficeSetupCompletePage();
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
          fileNames: ['file.docx'],
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogSpecificArgs: {
            oneDriveSetupDialogArgs: {
              setOfficeAsDefaultHandler: true,
            },
          },
        });

        await doWelcomePage(officeWebAppInstalled, odfsMounted);

        checkIsOfficeSetupCompletePage();
      });

  /**
   * Tests that when the Office PWA is already installed and ODFS is already
   * mounted, but there is no file to upload. If Office file handlers still need
   * to be set (first time setup), the welcome page should not be skipped, as
   * opposed to the Office PWA install page and the sign in page.
   */
  test(
      'Set up Office with PWA already installed, already signed in, no file' +
          ' to upload',
      async () => {
        const officeWebAppInstalled = true;
        const odfsMounted = true;
        await setUp({
          fileNames: [],
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogSpecificArgs: {
            oneDriveSetupDialogArgs: {
              setOfficeAsDefaultHandler: true,
            },
          },
        });

        await doWelcomePage(officeWebAppInstalled, odfsMounted);

        checkIsOfficeSetupCompletePage();
      });

  /**
   * Tests that when the Office PWA is already installed and ODFS is already
   * mounted, but there is no file to upload. If file handlers have already been
   * set, the welcome page should be skipped, as well as the Office PWA install
   * page and the sign in page.
   */
  test(
      'Set up Office with PWA already installed, already signed in, no file' +
          ' to upload, file handlers already set',
      async () => {
        const officeWebAppInstalled = true;
        const odfsMounted = true;
        await setUp({
          fileNames: [],
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogSpecificArgs: {
            oneDriveSetupDialogArgs: {
              setOfficeAsDefaultHandler: false,
            },
          },
        });

        checkIsOfficeSetupCompletePage();
      });

  /**
   * Tests that clicking the done button triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Open file button', async () => {
    const officeWebAppInstalled = false;
    const odfsMounted = false;
    await setUp({
      fileNames: ['file.docx'],
      officeWebAppInstalled,
      installOfficeWebAppResult: true,
      odfsMounted,
      dialogSpecificArgs: {
        oneDriveSetupDialogArgs: {
          setOfficeAsDefaultHandler: true,
        },
      },
    });
    await doWelcomePage(officeWebAppInstalled, odfsMounted);

    // Click the 'next' button on the welcome page.
    cloudUploadApp.$('.action-button').click();

    await doPWAInstallPage();
    await doSignInPage();

    checkIsOfficeSetupCompletePage();
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
   * except the last page, when there is no cancel button.
   */
  [1, 2, 3, 4].forEach(
      page => test(`Close button on page ${page}`, async () => {
        const officeWebAppInstalled = false;
        const odfsMounted = false;
        await setUp({
          fileNames: ['file.docx'],
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogSpecificArgs: {
            oneDriveSetupDialogArgs: {
              setOfficeAsDefaultHandler: true,
            },
          },
        });

        // Go to the specified page.
        if (page > 1) {
          await doWelcomePage(officeWebAppInstalled, odfsMounted);
        }
        if (page > 2) {
          await doPWAInstallPage();
        }

        if (page > 3) {
          await doSignInPage();
          // No cancel button should be shown on the last page.
          assertEquals(cloudUploadApp.$('.cancel-button'), null);
          return;
        }

        // Bring up the cancel dialog with a Cancel click.
        cloudUploadApp.$('.cancel-button').click();
        const cancelDialog =
            cloudUploadApp.$<SetupCancelDialogElement>('setup-cancel-dialog')!;
        assertTrue(cancelDialog.open);

        // Dismiss the cancel dialog with a Continue click.
        cancelDialog.$('.action-button').click();
        assertFalse(cancelDialog.open);

        // Bring up the cancel dialog with a Cancel click.
        cloudUploadApp.$('.cancel-button').click();
        assertTrue(cancelDialog.open);
        assertEquals(
            0, testProxy.handler.getCallCount('respondWithUserActionAndClose'));

        // Cancel setup with Cancel click.
        cancelDialog.$('.cancel-button').click();
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
        assertDeepEquals(
            [UserAction.kCancel],
            testProxy.handler.getArgs('respondWithUserActionAndClose'));
      }));

  /**
   * Tests that an Escape keydown should show the cancel dialog on each page
   * except the last page, where the setup is cancelled directly.
   */
  [1, 2, 3, 4].forEach(
      page => test(`Escape on page ${page}`, async () => {
        const officeWebAppInstalled = false;
        const odfsMounted = false;
        await setUp({
          fileNames: ['file.docx'],
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogSpecificArgs: {
            oneDriveSetupDialogArgs: {
              setOfficeAsDefaultHandler: true,
            },
          },
        });

        // Go to the specified page.
        if (page > 1) {
          await doWelcomePage(officeWebAppInstalled, odfsMounted);
        }
        if (page > 2) {
          await doPWAInstallPage();
        }

        const cancelDialog =
            cloudUploadApp.$<SetupCancelDialogElement>('setup-cancel-dialog')!;

        if (page > 3) {
          await doSignInPage();
          // Escape on the last page should not bring up the cancel dialog, but
          // it instead directly cancels the setup.
          document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
          await whenCheck(cancelDialog, () => !cancelDialog.open);
        } else {
          // Bring up the cancel dialog with an Escape keydown.
          document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
          await whenCheck(cancelDialog, () => cancelDialog.open);

          // Dismiss the cancel dialog with an Escape keydown.
          document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
          await whenCheck(cancelDialog, () => !cancelDialog.open);

          // Bring up the cancel dialog with an Escape keydown.
          document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
          await whenCheck(cancelDialog, () => cancelDialog.open);
          assertEquals(
              0,
              testProxy.handler.getCallCount('respondWithUserActionAndClose'));

          // Cancel setup with Cancel click.
          cancelDialog.$('.cancel-button').click();
        }

        // Check the setup was cancelled.
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
          fileNames: ['file.docx'],
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogSpecificArgs: {
            oneDriveSetupDialogArgs: {
              setOfficeAsDefaultHandler: true,
            },
          },
        });
        // Go to the OneDrive upload page.
        await doWelcomePage(officeWebAppInstalled, odfsMounted);
        await doPWAInstallPage();
        checkIsOfficeSetupCompletePage();

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
          fileNames: ['file.docx'],
          officeWebAppInstalled,
          installOfficeWebAppResult: true,
          odfsMounted,
          dialogSpecificArgs: {
            oneDriveSetupDialogArgs: {
              setOfficeAsDefaultHandler: false,
            },
          },
        });
        // Go to the OneDrive upload page.
        await doWelcomePage(officeWebAppInstalled, odfsMounted);
        await doPWAInstallPage();
        checkIsOfficeSetupCompletePage();

        // Click the open file button.
        cloudUploadApp.$('.action-button').click();

        assertEquals(
            0, testProxy.handler.getCallCount('setOfficeAsDefaultHandler'));
      });
});
