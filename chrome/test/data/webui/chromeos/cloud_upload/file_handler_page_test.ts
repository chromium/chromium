// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/file_handler_page.js';

import {DialogPage, DialogTask, OperationType, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {AccordionTopCardElement} from 'chrome://cloud-upload/file_handler_card.js';
import {FileHandlerPageElement} from 'chrome://cloud-upload/file_handler_page.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {CloudUploadTestBrowserProxy, ProxyOptions} from './cloud_upload_test_browser_proxy.js';

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

    // Setup fake strings.
    loadTimeData.resetForTesting({
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
    });

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
    loadTimeData.resetForTesting();
    assert(window.trustedTypes);
    container.innerHTML = window.trustedTypes.emptyHTML;
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
      fileNames: ['file.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      operationType: OperationType.kMove,
      localTasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
    assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
    assertTrue(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
    fileHandlerPageApp.$('#drive').click();
    assertFalse(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
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
      fileNames: ['file.docx'],
      officeWebAppInstalled: false,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      operationType: OperationType.kMove,
      localTasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
    assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
    assertTrue(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
    fileHandlerPageApp.$('#drive').click();
    assertFalse(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
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
      fileNames: ['file.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      operationType: OperationType.kMove,
      localTasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
    assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
    assertTrue(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
    fileHandlerPageApp.$('#onedrive').click();
    assertFalse(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
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
      fileNames: ['file.docx'],
      officeWebAppInstalled: false,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      operationType: OperationType.kMove,
      localTasks: createTasks(numTasks),
    });

    assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
    assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
    assertTrue(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
    fileHandlerPageApp.$('#onedrive').click();
    assertFalse(
        fileHandlerPageApp.$<CrButtonElement>('.action-button').disabled);
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
              fileNames: ['file.docx'],
              officeWebAppInstalled: true,
              installOfficeWebAppResult: false,
              odfsMounted: false,
              dialogPage: DialogPage.kFileHandlerDialog,
              operationType: OperationType.kMove,
              localTasks: createTasks(numTasks),
            });
            const accordionCard =
                fileHandlerPageApp.$<AccordionTopCardElement>('#accordion');
            const localTaskCard = fileHandlerPageApp.$('#id' + taskPosition);
            const actionButton =
                fileHandlerPageApp.$<CrButtonElement>('.action-button');
            assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
            assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
            // The accordion is initially collapsed and the task card is hidden.
            assertFalse(accordionCard.expanded);
            assertEquals(localTaskCard.style.display, 'none');
            // Expand the accordion first and select the local task.
            accordionCard.click();
            assertTrue(accordionCard.expanded);
            assertNotEquals(localTaskCard.style.display, 'none');
            assertTrue(actionButton.disabled);
            localTaskCard.click();
            assertFalse(actionButton.disabled);
            actionButton.click();
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
              fileNames: ['file.docx'],
              officeWebAppInstalled: false,
              installOfficeWebAppResult: false,
              odfsMounted: false,
              dialogPage: DialogPage.kFileHandlerDialog,
              operationType: OperationType.kMove,
              localTasks: createTasks(numTasks),
            });
            const accordionCard =
                fileHandlerPageApp.$<AccordionTopCardElement>('#accordion');
            const localTaskCard = fileHandlerPageApp.$('#id' + taskPosition);
            const actionButton =
                fileHandlerPageApp.$<CrButtonElement>('.action-button');
            assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
            assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
            // The accordion is initially collapsed and the task card is hidden.
            assertFalse(accordionCard.expanded);
            assertEquals(localTaskCard.style.display, 'none');
            // Expand the accordion first and select the local task.
            accordionCard.click();
            assertTrue(accordionCard.expanded);
            assertNotEquals(localTaskCard.style.display, 'none');
            assertTrue(actionButton.disabled);
            localTaskCard.click();
            assertFalse(actionButton.disabled);
            actionButton.click();
            await testProxy.handler.whenCalled('respondWithLocalTaskAndClose');
            assertEquals(
                1,
                testProxy.handler.getCallCount('respondWithLocalTaskAndClose'));
            assertDeepEquals(
                [taskPosition],
                testProxy.handler.getArgs('respondWithLocalTaskAndClose'));
          }));

  /** Test that the accordion doesn't show when there are no local tasks.*/
  test(`No accordion when no local task`, async () => {
    const numTasks = 0;
    await setUp({
      fileNames: ['file.docx'],
      officeWebAppInstalled: false,
      installOfficeWebAppResult: false,
      odfsMounted: false,
      dialogPage: DialogPage.kFileHandlerDialog,
      operationType: OperationType.kMove,
      localTasks: [],
    });
    assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
    assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
    assertFalse(!!fileHandlerPageApp.$('#accordion'));
  });

  /**
   * Test that any selected local task gets unselected if the accordion gets
   * collapsed.
   */
  test(
      `Collapsing the accordion unselects any selected local task`,
      async () => {
        const numTasks = 1;
        await setUp({
          fileNames: ['file.docx'],
          officeWebAppInstalled: false,
          installOfficeWebAppResult: false,
          odfsMounted: false,
          dialogPage: DialogPage.kFileHandlerDialog,
          operationType: OperationType.kMove,
          localTasks: createTasks(numTasks),
        });
        const accordionCard =
            fileHandlerPageApp.$<AccordionTopCardElement>('#accordion');
        const localTaskCard = fileHandlerPageApp.$('#id0');
        const actionButton =
            fileHandlerPageApp.$<CrButtonElement>('.action-button');
        assertEquals(fileHandlerPageApp.cloudProviderCards.length, 2);
        assertEquals(fileHandlerPageApp.localHandlerCards.length, numTasks);
        assertTrue(!!accordionCard);
        // The accordion is initially collapsed and the task card is hidden.
        assertFalse(accordionCard.expanded);
        assertEquals(localTaskCard.style.display, 'none');
        // Expand the accordion first and select the local task.
        accordionCard.click();
        assertTrue(accordionCard.expanded);
        assertNotEquals(localTaskCard.style.display, 'none');
        assertTrue(actionButton.disabled);
        localTaskCard.click();
        assertFalse(actionButton.disabled);
        // Collapse the accordion.
        accordionCard.click();
        assertFalse(accordionCard.expanded);
        assertEquals(localTaskCard.style.display, 'none');
        assertTrue(actionButton.disabled);
      });
});
