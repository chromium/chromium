// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/cloud_upload_dialog.js';
// TODO(cassycc): Check this okay to put here.
import 'chrome://cloud-upload/file_handler_page.js';

import {DialogArgs, DialogPage, DialogTask, PageHandlerRemote, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudUploadElement} from 'chrome://cloud-upload/cloud_upload_dialog.js';
import {FileHandlerPageElement} from 'chrome://cloud-upload/file_handler_page.js';
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
  dialogPage: DialogPage;
  tasks?: DialogTask[]|null;
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
      dialogPage: options.dialogPage,
      tasks: [],
    };
    if (options.fileName != null) {
      args.fileNames.push(options.fileName);
    }
    if (options.tasks != null) {
      args.tasks = options.tasks;
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
      dialogPage: DialogPage.kOneDriveSetup,
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
      dialogPage: DialogPage.kOneDriveSetup,
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
      dialogPage: DialogPage.kOneDriveSetup,
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
      dialogPage: DialogPage.kOneDriveSetup,
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
          dialogPage: DialogPage.kOneDriveSetup,
        });

        await doWelcomePage();

        checkIsOneDriveUploadPage();
      });

  /**
   * Tests that clicking the open file button triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Open file button', async () => {
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: true,
      odfsMounted: false,
      dialogPage: DialogPage.kOneDriveSetup,
    });
    checkIsWelcomePage();

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
   * Tests that clicking the close button on the last page triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Close button on last page', async () => {
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: true,
      odfsMounted: false,
      dialogPage: DialogPage.kOneDriveSetup,
    });

    // Go to the OneDrive upload page.
    await doWelcomePage();
    await doPWAInstallPage();
    await doSignInPage();

    checkIsOneDriveUploadPage();
    cloudUploadApp.$('.cancel-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kCancel],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
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
          dialogPage: DialogPage.kOneDriveSetup,
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
});

suite('<file-handler-page>', () => {
  /* Holds the <file-handler-page> app. */
  let container: HTMLDivElement;
  /* The <file-handler-page> app. */
  let fileHandlerPageApp: FileHandlerPageElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testProxy: CloudUploadTestBrowserProxy;

  async function setUp(options: ProxyOptions) {
    testProxy = new CloudUploadTestBrowserProxy(options);
    CloudUploadBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <file-handler-page> element to the DOM tree.
    fileHandlerPageApp =
        document.createElement('file-handler-page') as FileHandlerPageElement;
    container.appendChild(fileHandlerPageApp);
    await fileHandlerPageApp.initDynamicContent;
  }

  /**
   * Create `numTasks` number of `DialogTask` tasks with each task having a
   * unique `position`.
   */
  function createTasks(numTasks: number): DialogTask[] {
    const tasks: DialogTask[] = [];
    const title = 'title';
    const iconUrl = 'iconUrl';
    const appId = 'appId';
    for (let i = 0; i < numTasks; i++) {
      const position = i;
      tasks.push({title, position, appId, iconUrl});
    }
    return tasks;
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
   * the <file-handler-page> component.
   */
  teardown(() => {
    container.innerHTML = '';
    testProxy.handler.reset();
  });

  /**
   * Test that clicking the Drive App and then the open file button triggers the
   * right `respondWithUserActionAndClose` mojo request when the Office PWA is
   * installed and there are local file tasks.
   */
  test('Open file with Drive when Office PWA installed', async () => {
    const numTasks = 5;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      tasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.tasks.length, numTasks);
    fileHandlerPageApp.$('#drive').click();
    fileHandlerPageApp.$('.action-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kConfirmOrUploadToGoogleDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });


  /**
   * Test that clicking the Drive App and then the open file button triggers the
   * right `respondWithUserActionAndClose` mojo request when the Office PWA is
   * not installed and there are local file tasks.
   */
  test('Open file with Drive when Office PWA not installed', async () => {
    const numTasks = 5;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      tasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.tasks.length, numTasks);
    fileHandlerPageApp.$('#drive').click();
    fileHandlerPageApp.$('.action-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kConfirmOrUploadToGoogleDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });

  /**
   * Test that clicking the OneDrive App and then the open file button triggers
   * the right `respondWithUserActionAndClose` mojo request when the Office PWA
   * is installed and there are local file tasks.
   */
  test('Open file with OneDrive when Office PWA installed', async () => {
    const numTasks = 5;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      tasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.tasks.length, numTasks);
    fileHandlerPageApp.$('#onedrive').click();
    fileHandlerPageApp.$('.action-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kSetUpOneDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });


  /**
   * Test that clicking the OneDrive App and then the open file button triggers
   * the right `respondWithUserActionAndClose` mojo request when the Office PWA
   * is not installed and there are local file tasks.
   */
  test('Open file with OneDrive when Office PWA not installed', async () => {
    const numTasks = 5;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      tasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.tasks.length, numTasks);
    fileHandlerPageApp.$('#onedrive').click();
    fileHandlerPageApp.$('.action-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kSetUpOneDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });

  /**
   * For each created local task, test that clicking that task and then the open
   * file button triggers the right `respondWithLocalTaskAndClose` mojo request
   * when the Office PWA is installed.
   */
  [0, 1, 2, 3, 4].forEach(
      taskPosition => test(
          `Open file with local task ${taskPosition} when Office PWA installed`,
          async () => {
            const numTasks = 5;
            await setUp({
              fileName: 'file.docx',
              officeWebAppInstalled: true,
              installOfficeWebAppResult: false,
              odfsMounted: false,
              dialogPage: DialogPage.kFileHandlerDialog,
              tasks: createTasks(numTasks),
            });
            assertEquals(fileHandlerPageApp.tasks.length, numTasks);
            fileHandlerPageApp.$('#id' + taskPosition).click();
            fileHandlerPageApp.$('.action-button').click();
            await testProxy.handler.whenCalled('respondWithLocalTaskAndClose');
            assertEquals(
                1,
                testProxy.handler.getCallCount('respondWithLocalTaskAndClose'));
            assertDeepEquals(
                [taskPosition],
                testProxy.handler.getArgs('respondWithLocalTaskAndClose'));
          }));

  /**
   * For each created local task, test that clicking that task and then the open
   * file button triggers the right `respondWithLocalTaskAndClose` mojo request
   * when the Office PWA is not installed.
   */
  [0, 1, 2, 3, 4].forEach(
      taskPosition => test(
          `Open file with local task ${
              taskPosition} when Office PWA not installed`,
          async () => {
            const numTasks = 5;
            await setUp({
              fileName: 'file.docx',
              officeWebAppInstalled: false,
              installOfficeWebAppResult: false,
              odfsMounted: false,
              dialogPage: DialogPage.kFileHandlerDialog,
              tasks: createTasks(numTasks),
            });
            assertEquals(fileHandlerPageApp.tasks.length, numTasks);
            fileHandlerPageApp.$('#id' + taskPosition).click();
            fileHandlerPageApp.$('.action-button').click();
            await testProxy.handler.whenCalled('respondWithLocalTaskAndClose');
            assertEquals(
                1,
                testProxy.handler.getCallCount('respondWithLocalTaskAndClose'));
            assertDeepEquals(
                [taskPosition],
                testProxy.handler.getArgs('respondWithLocalTaskAndClose'));
          }));

  /** Test that the dialog doesn't crash when there are no local tasks.*/
  test(`No local task`, async () => {
    const numTasks = 0;
    await setUp({
      fileName: 'file.docx',
      officeWebAppInstalled: false,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      tasks: [],
    });
    assertEquals(fileHandlerPageApp.tasks.length, numTasks);
  });
});