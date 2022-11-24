// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://office-fallback/office_fallback_dialog.js';

import {DialogChoice, PageHandlerRemote} from 'chrome://office-fallback/office_fallback.mojom-webui.js';
import {OfficeFallbackBrowserProxy} from 'chrome://office-fallback/office_fallback_browser_proxy.js';
import type {OfficeFallbackElement} from 'chrome://office-fallback/office_fallback_dialog.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

interface ProxyOptions {
  fileNames?: string[];
  taskTitle: string;
  // The string will be converted to enum FallbackReason from
  // office_fallback_dialog.ts in the mapping in
  // OfficeFallbackElement.stringToFailureReason.
  fallbackReason: string;
}

/**
 * A test OfficeFallbackBrowserProxy implementation that enables to mock various
 * mojo responses.
 */
class OfficeFallbackTestBrowserProxy implements OfficeFallbackBrowserProxy {
  handler: PageHandlerRemote&TestBrowserProxy;
  dialogArgs: string;

  constructor(options: ProxyOptions) {
    this.handler = TestBrowserProxy.fromClass(PageHandlerRemote);
    // Creating JSON string as in OfficeFallbackDialog::GetDialogArgs().
    const args = {
      'fileNames': options.fileNames,
      'fallbackReason': options.fallbackReason,
      'taskTitle': options.taskTitle,
    };
    this.dialogArgs = JSON.stringify(args);
  }

  // JSON-encoded dialog arguments.
  getDialogArguments(): string {
    return this.dialogArgs;
  }
}

suite('<office-fallback>', () => {
  // Holds the <cloud-upload> app.
  let container: HTMLDivElement;
  // The <office-fallback> app.
  let officeFallbackApp: OfficeFallbackElement;
  // The BrowserProxy element to make assertions on when mojo methods are
  // called.
  let testProxy: OfficeFallbackTestBrowserProxy;

  const setUp = async (options: ProxyOptions) => {
    testProxy = new OfficeFallbackTestBrowserProxy(options);
    OfficeFallbackBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <office-fallback> element to the DOM tree.
    officeFallbackApp = document.createElement('office-fallback');
    container.appendChild(officeFallbackApp);
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
   * Tests that clicking the "quick office" button triggers the right `close`
   * mojo request.
   */
  test('Open in offline editor button', async () => {
    await setUp({
      fileNames: ['file.docx'],
      taskTitle: 'testTitle',
      fallbackReason: 'Offline',
    });

    officeFallbackApp.$('#quick-office-button').click();
    await testProxy.handler.whenCalled('close');
    assertEquals(1, testProxy.handler.getCallCount('close'));
    assertDeepEquals(
        [DialogChoice.kQuickOffice], testProxy.handler.getArgs('close'));
  });

  /**
   * Tests that clicking the "try again" button triggers the right `close`
   * mojo request.
   */
  test('Try again button', async () => {
    await setUp({
      fileNames: ['file.docx'],
      taskTitle: 'testTitle',
      fallbackReason: 'Drive Unavailable',
    });

    officeFallbackApp.$('#try-again-button').click();
    await testProxy.handler.whenCalled('close');
    assertEquals(1, testProxy.handler.getCallCount('close'));
    assertDeepEquals(
        [DialogChoice.kTryAgain], testProxy.handler.getArgs('close'));
  });

  /**
   * Tests that clicking the "close" button triggers the right `close`
   * mojo request.
   */
  test('Close button', async () => {
    await setUp({
      fileNames: ['file.docx'],
      taskTitle: 'testTitle',
      fallbackReason: 'OneDrive Unavailable',
    });

    officeFallbackApp.$('#cancel-button').click();
    await testProxy.handler.whenCalled('close');
    assertEquals(1, testProxy.handler.getCallCount('close'));
    assertDeepEquals(
        [DialogChoice.kCancel], testProxy.handler.getArgs('close'));
  });


  /**
   * Tests that the fileNames are displayed correctly in the title.
   */
  test('fileNames', async () => {
    let fileName = `file1.docx`;
    const fileNames = [fileName];
    for (let i = 2; i < 12; i++) {
      fileName = `file${i}.docx`;
      fileNames.push(fileName);
    }

    await setUp({
      fileNames: fileNames,
      taskTitle: 'testTitle',
      fallbackReason: 'Offline',
    });

    const title = officeFallbackApp.$('#title').innerText;
    fileNames.forEach((fileName) => {
      assertTrue(title.includes(fileName));
    });
  });
});