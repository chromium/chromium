// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/file_handler_page.js';

import {DialogPage, DialogTask, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {FileHandlerPageElement} from 'chrome://cloud-upload/file_handler_page.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

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