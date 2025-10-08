// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import type {PdfNavigator, ViewerSaveToDriveBubbleElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {WindowOpenDisposition} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setUpTestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';
import {getRequiredElement} from './test_util.js';

const SaveToDriveStatus = chrome.pdfViewerPrivate.SaveToDriveStatus;
const SaveToDriveErrorType = chrome.pdfViewerPrivate.SaveToDriveErrorType;

const viewer = document.body.querySelector('pdf-viewer')!;

class TestPdfNavigator extends TestBrowserProxy implements PdfNavigator {
  constructor() {
    super([
      'navigate',
    ]);
  }

  navigate(urlString: string, disposition: WindowOpenDisposition):
      Promise<void> {
    this.methodCalled('navigate', urlString, disposition);
    return Promise.resolve();
  }
}

function assertBubbleAndProgressBar(
    bubble: ViewerSaveToDriveBubbleElement, value: number, max: number): void {
  chrome.test.assertTrue(bubble.$.dialog.open);
  const progressBar = getRequiredElement(bubble, 'cr-progress');
  chrome.test.assertEq(value, progressBar.value);
  chrome.test.assertEq(max, progressBar.max);
}

function assertBubbleDescription(
    bubble: ViewerSaveToDriveBubbleElement, description: string): void {
  const descriptionElement = getRequiredElement(bubble, '#description');
  chrome.test.assertTrue(!!descriptionElement.textContent);
  chrome.test.assertEq(description, descriptionElement.textContent.trim());
}

function assertNavigateUrl(
    navigator: TestPdfNavigator, redirectUrl: string): void {
  const expectedUrl = new URL('https://accounts.google.com/AccountChooser');
  expectedUrl.searchParams.append('Email', 'test@gmail.com');
  expectedUrl.searchParams.append('faa', '1');
  expectedUrl.searchParams.append('continue', redirectUrl);

  chrome.test.assertEq(1, navigator.getCallCount('navigate'));
  const args = navigator.getArgs('navigate');
  chrome.test.assertEq(1, args.length);
  const [url, disposition] = args[0];
  chrome.test.assertEq(expectedUrl.href, url);
  chrome.test.assertEq(WindowOpenDisposition.NEW_FOREGROUND_TAB, disposition);
}

function closeBubble(bubble: ViewerSaveToDriveBubbleElement): void {
  bubble.$.dialog.close();
  chrome.test.assertFalse(bubble.$.dialog.open);
}

function setUpTestNavigator(): TestPdfNavigator {
  const navigator = new TestPdfNavigator();
  viewer.setPdfNavigatorForTesting(navigator);
  return navigator;
}

async function testQuotaExceededState(
    accountIsManaged: boolean, expectedRedirectUrl: string) {
  const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
  const navigator = setUpTestNavigator();
  const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

  // Set quota exceeded state and open the bubble.
  privateProxy.sendQuotaExceededError(accountIsManaged);
  const controls =
      getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
  controls.$.save.click();
  await microtasksFinished();

  assertBubbleDescription(bubble, 'Your Google Drive storage is full');

  // Click the manage storage button in the bubble and verify the bubble
  // is closed.
  const button = getRequiredElement(bubble, '#manage-storage-button');
  button.click();
  chrome.test.assertFalse(bubble.$.dialog.open);

  // Verify the url passed to the navigator.
  assertNavigateUrl(navigator, expectedRedirectUrl);

  // Manage storage click should reset the state, so clicking on the save
  // button again to make sure it initiates a new upload.
  chrome.test.assertEq(0, privateProxy.getCallCount('saveToDrive'));
  controls.$.save.click();
  await privateProxy.whenCalled('saveToDrive');
  chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));
  chrome.test.assertFalse(bubble.$.dialog.open);
}

// Unit tests for the pdf-viewer Save to Drive elements.
const tests = [
  async function testSaveToDriveButton() {
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);

    chrome.test.assertEq('pdf:add-to-drive', controls.$.save.ironIcon);

    privateProxy.sendUploadInProgress(25, 100);
    await microtasksFinished();
    chrome.test.assertEq('pdf:arrow-upward-alt', controls.$.save.ironIcon);
    const progress = getRequiredElement(controls, 'circular-progress-ring');
    chrome.test.assertEq(25, progress.value);
    chrome.test.assertEq(
        '425px', progress.$.innerProgress.getAttribute('stroke-dashoffset'));

    privateProxy.sendSessionTimeoutError();
    await microtasksFinished();
    chrome.test.assertEq('pdf:add-to-drive', controls.$.save.ironIcon);
    chrome.test.assertFalse(
        !!controls.shadowRoot.querySelector('circular-progress-ring'));

    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');
    chrome.test.assertTrue(bubble.$.dialog.open);

    // Reset the bubble open state for the next test.
    closeBubble(bubble);

    chrome.test.succeed();
  },

  async function testSaveToDriveBubble() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    chrome.test.assertTrue(!!bubble);
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Save to drive uninitialized state should not show the bubble.
    privateProxy.sendUninitializedState();
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await microtasksFinished();
    const args = await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertEq(args, 'ORIGINAL');
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Save to drive uploading 0/100 bytes.
    privateProxy.sendUploadInProgress(0, 100);
    controls.$.save.click();
    await microtasksFinished();
    assertBubbleAndProgressBar(bubble, 0, 100);
    chrome.test.assertFalse(!!bubble.shadowRoot.querySelector('#retry-button'));
    const fileMetadata = getRequiredElement(bubble, '#file-metadata');
    chrome.test.assertTrue(!!fileMetadata.textContent);
    chrome.test.assertEq(
        'uploading, 2 minutes left', fileMetadata.textContent.trim());
    const filename = getRequiredElement(bubble, '#filename');
    chrome.test.assertTrue(!!filename.textContent);
    chrome.test.assertEq('test.pdf', filename.textContent.trim());

    // Save to drive uploading 88/226 bytes.
    privateProxy.sendUploadInProgress(88, 226);
    await microtasksFinished();
    assertBubbleAndProgressBar(bubble, 88, 226);

    // Save to drive uploading 226/226 bytes.
    privateProxy.sendUploadInProgress(226, 226);
    await microtasksFinished();
    assertBubbleAndProgressBar(bubble, 226, 226);

    // Save to drive with session timeout error should show the retry button and
    // hide the progress bar.
    privateProxy.sendSessionTimeoutError();
    await microtasksFinished();
    assertBubbleDescription(bubble, 'Session timed out');
    chrome.test.assertFalse(!!bubble.shadowRoot.querySelector('cr-progress'));
    chrome.test.assertTrue(!!bubble.shadowRoot.querySelector('#retry-button'));

    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));

    // Reset the bubble open state for the next test.
    closeBubble(bubble);

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleCloseButtonAndStateResets() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');

    // Set the save to Drive state to upload complete and open the bubble.
    privateProxy.sendUploadCompleted();
    controls.$.save.click();
    await microtasksFinished();
    chrome.test.assertTrue(bubble.$.dialog.open);
    chrome.test.assertEq(0, privateProxy.getCallCount('saveToDrive'));

    // Click the close button in the bubble and verify the bubble is closed.
    const closeButton = getRequiredElement(bubble, '#close');
    closeButton.click();
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Click on the save button again and make sure it initiates a new upload
    // and the bubble is not open.
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertFalse(bubble.$.dialog.open);
    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleCloseButtonNotResetting() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');

    // Set the save to Drive state to upload in progress.
    privateProxy.sendUploadInProgress(0, 100);
    controls.$.save.click();
    await microtasksFinished();
    chrome.test.assertTrue(bubble.$.dialog.open);

    // Click the close button in the bubble and verify the bubble is closed.
    const closeButton = getRequiredElement(bubble, '#close');
    closeButton.click();
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Click on the save button again. It should just open the bubble and not
    // initiate a new upload.
    controls.$.save.click();
    await microtasksFinished();
    chrome.test.assertTrue(bubble.$.dialog.open);
    chrome.test.assertEq(0, privateProxy.getCallCount('saveToDrive'));

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleCancelUpload() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Set the save to Drive state to uploading and open the bubble.
    privateProxy.sendUploadInProgress(0, 100);
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await microtasksFinished();
    assertBubbleAndProgressBar(bubble, 0, 100);

    // Click the cancel button in the bubble and verify the saveToDrive API is
    // called with the cancelUpload flag and the bubble is closed.
    const cancelButton = getRequiredElement(bubble, '#cancel-upload-button');
    cancelButton.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Cancel button click should reset the state, so click on the save button
    // again to make sure it initiates a new upload.
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertEq(2, privateProxy.getCallCount('saveToDrive'));
    chrome.test.assertFalse(bubble.$.dialog.open);
    const args = privateProxy.getArgs('saveToDrive');
    chrome.test.assertEq(2, args.length);
    // Cancel upload click.
    chrome.test.assertEq(null, args[0]);
    // Save button click.
    chrome.test.assertEq('ORIGINAL', args[1]);

    chrome.test.succeed();
  },

  async function testBubbleQuotaExceededAndManageStorageConsumerClick() {
    const redirectUrl = new URL('https://one.google.com/storage');
    redirectUrl.searchParams.append('utm_source', 'drive');
    redirectUrl.searchParams.append('utm_medium', 'desktop');
    redirectUrl.searchParams.append('utm_campaign', 'error_dialog_oos');

    await testQuotaExceededState(/* accountIsManaged= */ false,
                                 redirectUrl.href);

    chrome.test.succeed();
  },

  async function testBubbleQuotaExceededAndManageStorageDasherClick() {
    await testQuotaExceededState(/* accountIsManaged= */ true,
                                 'https://drive.google.com/drive/quota');

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleUploadCompletedAndOpenInDriveClick() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const navigator = setUpTestNavigator();
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Set upload completed state and open the bubble.
    privateProxy.sendUploadCompleted();
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await microtasksFinished();

    assertBubbleDescription(
        bubble, 'Saved to your test-parent-folder-name folder');
    const filename = getRequiredElement(bubble, '#filename');
    chrome.test.assertTrue(!!filename.textContent);
    chrome.test.assertEq('save_to_drive_test.pdf', filename.textContent.trim());

    // Click the open in Drive button in the bubble and verify the bubble is
    // closed.
    const button = getRequiredElement(bubble, '#open-in-drive-button');
    button.click();
    await navigator.whenCalled('navigate');
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Verify the url passed to the navigator.
    const redirectUrl = new URL('https://drive.google.com/');
    redirectUrl.searchParams.append('action', 'locate');
    redirectUrl.searchParams.append('id', 'test-drive-item-id');
    assertNavigateUrl(navigator, redirectUrl.href);

    // Open in Drive click should reset the state. Clicking on the save button
    // again and make sure it initiates a new upload.
    chrome.test.assertEq(0, privateProxy.getCallCount('saveToDrive'));
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));
    chrome.test.assertFalse(bubble.$.dialog.open);

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleRetryUploadOriginal() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Click on the save button to initiate an upload.
    privateProxy.sendUninitializedState();
    await microtasksFinished();
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');

    // Set the save to Drive state to session timeout error state and open the
    // bubble.
    privateProxy.sendSessionTimeoutError();
    controls.$.save.click();
    await microtasksFinished();

    // Click the retry button in the bubble.
    const retryButton = getRequiredElement(bubble, '#retry-button');
    retryButton.click();
    await privateProxy.whenCalled('saveToDrive');

    chrome.test.assertEq(2, privateProxy.getCallCount('saveToDrive'));
    const args = privateProxy.getArgs('saveToDrive');
    chrome.test.assertEq(2, args.length);
    chrome.test.assertEq('ORIGINAL', args[0]);
    chrome.test.assertEq('ORIGINAL', args[1]);

    // Reset the bubble open state for the next test.
    closeBubble(bubble);

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleRetryUploadEdited() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Click on the save button to initiate an edited upload.
    privateProxy.sendUninitializedState();
    await microtasksFinished();
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.hasEdits = true;
    controls.$.save.click();
    await microtasksFinished();
    const buttons = controls.shadowRoot.querySelectorAll('button');
    buttons[0]!.click();
    await privateProxy.whenCalled('saveToDrive');

    // Set the save to Drive state to session timeout error state and open the
    // bubble.
    privateProxy.sendSessionTimeoutError();
    controls.$.save.click();
    await microtasksFinished();

    // Click the retry button in the bubble.
    const retryButton = getRequiredElement(bubble, '#retry-button');
    retryButton.click();
    await privateProxy.whenCalled('saveToDrive');

    // Click on the save button again after `hasEdits` is false to reset the
    // internal `saveToDriveRequestType_`, or else it will enable beforeunload
    // dialog in the next test.
    privateProxy.sendUninitializedState();
    controls.hasEdits = false;
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');

    // Reset the bubble open state for the next test.
    closeBubble(bubble);

    chrome.test.assertEq(3, privateProxy.getCallCount('saveToDrive'));
    const args = privateProxy.getArgs('saveToDrive');
    chrome.test.assertEq(3, args.length);
    chrome.test.assertEq('EDITED', args[0]);
    chrome.test.assertEq('EDITED', args[1]);
    chrome.test.assertEq('ORIGINAL', args[2]);

    chrome.test.succeed();
  },

  async function testBubbleOpenAndCloseAutomaticallyAfterUploadInProgress() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    privateProxy.sendUploadInProgress(0, 100);
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    privateProxy.sendUploadInProgress(50, 100);
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    const mockTimer = new MockTimer();
    mockTimer.install();

    privateProxy.sendSessionTimeoutError();
    await bubble.updateComplete;
    chrome.test.assertTrue(bubble.$.dialog.open);

    mockTimer.tick(2000);
    await bubble.updateComplete;
    chrome.test.assertTrue(bubble.$.dialog.open);

    mockTimer.tick(3000);
    await bubble.updateComplete;
    chrome.test.assertFalse(bubble.$.dialog.open);

    mockTimer.uninstall();

    chrome.test.succeed();
  },

  async function testBubbleNotCloseAfterUploadInProgressIfOpenedManually() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    privateProxy.sendUploadInProgress(0, 100);
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await microtasksFinished();
    assertBubbleAndProgressBar(bubble, 0, 100);

    const mockTimer = new MockTimer();
    mockTimer.install();

    privateProxy.sendSessionTimeoutError();
    await bubble.updateComplete;
    chrome.test.assertTrue(bubble.$.dialog.open);

    mockTimer.tick(6000);
    await bubble.updateComplete;
    chrome.test.assertTrue(bubble.$.dialog.open);

    mockTimer.uninstall();

    // Reset the bubble open state for the next test.
    closeBubble(bubble);

    chrome.test.succeed();
  },

  async function testStateResetsAfterAccountChooserCanceled() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    privateProxy.sendUploadInProgress(0, 100);
    await microtasksFinished();
    privateProxy.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_FAILED,
      errorType: SaveToDriveErrorType.ACCOUNT_CHOOSER_CANCELED,
    });
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Click on the save button again and make sure it initiates a new upload
    // and the bubble is not open.
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertFalse(bubble.$.dialog.open);
    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleUploadInitialized() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');

    privateProxy.sendSaveToDriveProgress({
      status: SaveToDriveStatus.INITIATED,
      errorType: SaveToDriveErrorType.NO_ERROR,
    });
    await microtasksFinished();
    controls.$.save.click();
    await microtasksFinished();
    const progress = getRequiredElement(controls, 'circular-progress-ring');
    chrome.test.assertEq(0, progress.value);
    chrome.test.assertEq(
        '566px', progress.$.innerProgress.getAttribute('stroke-dashoffset'));
    assertBubbleAndProgressBar(bubble, 0, 0);

    // Reset the bubble and the upload for the next test.
    privateProxy.sendUninitializedState();
    await microtasksFinished();
    closeBubble(bubble);

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleConnectionError() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');

    privateProxy.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_FAILED,
      errorType: SaveToDriveErrorType.OFFLINE,
    });
    await microtasksFinished();
    controls.$.save.click();
    await microtasksFinished();

    assertBubbleDescription(bubble, 'Check your internet connection');

    // Reset the bubble and the upload for the next test.
    privateProxy.sendUninitializedState();
    await microtasksFinished();
    closeBubble(bubble);

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleConnectionError() {
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');

    privateProxy.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_FAILED,
      errorType: SaveToDriveErrorType.OFFLINE,
    });
    await microtasksFinished();
    controls.$.save.click();
    await microtasksFinished();

    assertBubbleDescription(bubble, 'Check your internet connection');

    // Reset the bubble open state for the next test.
    closeBubble(bubble);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
