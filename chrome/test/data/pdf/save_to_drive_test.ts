// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import type {PdfViewerPrivateProxy, ViewerSaveToDriveBubbleElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {PdfViewerPrivateProxyImpl} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getRequiredElement} from './test_util.js';

const SaveToDriveStatus = chrome.pdfViewerPrivate.SaveToDriveStatus;
const SaveToDriveErrorType = chrome.pdfViewerPrivate.SaveToDriveErrorType;

const viewer = document.body.querySelector('pdf-viewer')!;

export class TestPdfViewerPrivateProxy extends TestBrowserProxy implements
    PdfViewerPrivateProxy {
  onSaveToDriveProgress: FakeChromeEvent;
  private streamUrl_: string = '';

  constructor() {
    super([
      'saveToDrive',
      'setPdfDocumentTitle',
    ]);

    this.onSaveToDriveProgress = new FakeChromeEvent();
  }

  sendSaveToDriveProgress(
      progress: chrome.pdfViewerPrivate.SaveToDriveProgress): void {
    this.onSaveToDriveProgress.callListeners(this.streamUrl_, progress);
  }

  saveToDrive(saveRequestType?: chrome.pdfViewerPrivate.SaveRequestType): void {
    this.methodCalled('saveToDrive', saveRequestType);
  }

  setPdfDocumentTitle(title: string): void {
    this.methodCalled('setPdfDocumentTitle', title);
  }

  setStreamUrl(streamUrl: string): void {
    this.streamUrl_ = streamUrl;
  }

  sendQuotaExceededError(): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.QUOTA_EXCEEDED,
      accountEmail: 'test@gmail.com',
    });
  }

  sendSessionTimeoutError(): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.OFFLINE,
    });
  }

  sendUploadInProgress(uploadedBytes: number, fileSizeBytes: number): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.NO_ERROR,
      uploadedBytes: uploadedBytes,
      fileSizeBytes: fileSizeBytes,
      fileMetadata: 'uploading, 2 minutes left',
    });
  }

  sendUploadCompleted(): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_COMPLETED,
      errorType: SaveToDriveErrorType.NO_ERROR,
      driveItemId: 'test-drive-item-id',
      parentFolderName: 'test-parent-folder-name',
      fileName: 'save_to_drive_test.pdf',
      accountEmail: 'test@gmail.com',
    });
  }

  sendUninitializedState(): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.NOT_STARTED,
      errorType: SaveToDriveErrorType.NO_ERROR,
    });
  }
}

function assertBubbleAndProgressBar(
    bubble: ViewerSaveToDriveBubbleElement, value: number, max: number): void {
  chrome.test.assertTrue(bubble.$.dialog.open);
  const progressBar = getRequiredElement(bubble, 'cr-progress');
  chrome.test.assertEq(value, progressBar.value);
  chrome.test.assertEq(max, progressBar.max);
}

function closeBubble(bubble: ViewerSaveToDriveBubbleElement): void {
  bubble.$.dialog.close();
  chrome.test.assertFalse(bubble.$.dialog.open);
}

function setUpTestPrivateProxy(): TestPdfViewerPrivateProxy {
  const privateProxy = new TestPdfViewerPrivateProxy();
  privateProxy.setStreamUrl(viewer.getStreamUrlForTesting());
  PdfViewerPrivateProxyImpl.setInstance(privateProxy);
  viewer.setOnSaveToDriveProgressListenerForTesting();
  return privateProxy;
}

// Unit tests for the pdf-viewer Save to Drive elements.
const tests = [
  async function testSaveToDriveButton() {
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    const privateProxy = setUpTestPrivateProxy();

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
    const privateProxy = setUpTestPrivateProxy();
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

    // Save to drive uploading with no bytes uploaded nor file size bytes.
    // Progress bar should not change.
    privateProxy.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.NO_ERROR,
    });
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
    chrome.test.assertFalse(!!bubble.shadowRoot.querySelector('cr-progress'));
    chrome.test.assertTrue(!!bubble.shadowRoot.querySelector('#retry-button'));

    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));

    // Reset the bubble open state for the next test.
    closeBubble(bubble);

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleCloseButtonAndStateResets() {
    const privateProxy = setUpTestPrivateProxy();
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
    const privateProxy = setUpTestPrivateProxy();
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
    const privateProxy = setUpTestPrivateProxy();
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

  async function testSaveToDriveBubbleQuotaExceededAndManageStorageClick() {
    const privateProxy = setUpTestPrivateProxy();
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Set quota exceeded state and open the bubble.
    privateProxy.sendQuotaExceededError();
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await microtasksFinished();

    const description = getRequiredElement(bubble, '#description');
    chrome.test.assertTrue(!!description.textContent);
    chrome.test.assertEq(
        'Your Google Drive storage is full', description.textContent.trim());

    // Click the manage storage button in the bubble and verify the bubble is
    // closed.
    const button = getRequiredElement(bubble, '#manage-storage-button');
    button.click();
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Manage storage click should reset the state, so clicking on the save
    // button again to make sure it initiates a new upload.
    chrome.test.assertEq(0, privateProxy.getCallCount('saveToDrive'));
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));
    chrome.test.assertFalse(bubble.$.dialog.open);

    // TODO(crbug.com/427451594): Write tests for clicking on the manage
    // storage button to test the URL is correct.

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleUploadCompletedAndOpenInDriveClick() {
    const privateProxy = setUpTestPrivateProxy();
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Set upload completed state and open the bubble.
    privateProxy.sendUploadCompleted();
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    controls.$.save.click();
    await microtasksFinished();

    const description = getRequiredElement(bubble, '#description');
    chrome.test.assertTrue(!!description.textContent);
    chrome.test.assertEq(
        'Saved to your test-parent-folder-name folder',
        description.textContent.trim());
    const filename = getRequiredElement(bubble, '#filename');
    chrome.test.assertTrue(!!filename.textContent);
    chrome.test.assertEq('save_to_drive_test.pdf', filename.textContent.trim());

    // Click the open in Drive button in the bubble and verify the bubble is
    // closed.
    const button = getRequiredElement(bubble, '#open-in-drive-button');
    button.click();
    await microtasksFinished();
    chrome.test.assertFalse(bubble.$.dialog.open);

    // Open in Drive click should reset the state. Clicking on the save button
    // again and make sure it initiates a new upload.
    chrome.test.assertEq(0, privateProxy.getCallCount('saveToDrive'));
    controls.$.save.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));
    chrome.test.assertFalse(bubble.$.dialog.open);

    // TODO(crbug.com/427451594): Write tests for clicking on the open in Drive
    // button to test the URL is correct.

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleRetryUploadOriginal() {
    const privateProxy = setUpTestPrivateProxy();
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
    const privateProxy = setUpTestPrivateProxy();
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

    chrome.test.assertEq(2, privateProxy.getCallCount('saveToDrive'));
    const args = privateProxy.getArgs('saveToDrive');
    chrome.test.assertEq(2, args.length);
    chrome.test.assertEq('EDITED', args[0]);
    chrome.test.assertEq('EDITED', args[1]);

    // Reset the bubble open state for the next test.
    closeBubble(bubble);
    controls.hasEdits = false;

    chrome.test.succeed();
  },

  async function testBubbleOpenAndCloseAutomaticallyAfterUploadInProgress() {
    const privateProxy = setUpTestPrivateProxy();
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
    const privateProxy = setUpTestPrivateProxy();
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
    const privateProxy = setUpTestPrivateProxy();
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
];

chrome.test.runTests(tests);
