// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import type {PdfViewerPrivateProxy, ViewerSaveToDriveBubbleElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {PdfViewerPrivateProxyImpl} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
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
      fileName: 'save_to_drive_test.pdf',
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
    assert(fileMetadata.textContent);
    chrome.test.assertEq(
        'uploading, 2 minutes left', fileMetadata.textContent.trim());
    const filename = getRequiredElement(bubble, '#filename');
    assert(filename.textContent);
    chrome.test.assertEq('save_to_drive_test.pdf', filename.textContent.trim());

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
    // called with the cancelUpload flag.
    const cancelButton = getRequiredElement(bubble, '#cancel-upload-button');
    cancelButton.click();
    await microtasksFinished();
    const args = await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertEq(args, null);

    chrome.test.assertEq(1, privateProxy.getCallCount('saveToDrive'));

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleRetryUploadOriginal() {
    const privateProxy = setUpTestPrivateProxy();
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Click on the save button to initiate an upload.
    privateProxy.sendUninitializedState();
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

    chrome.test.succeed();
  },

  async function testSaveToDriveBubbleRetryUploadEdited() {
    const privateProxy = setUpTestPrivateProxy();
    const bubble = getRequiredElement(viewer, 'viewer-save-to-drive-bubble');

    // Click on the save button to initiate an edited upload.
    privateProxy.sendUninitializedState();
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

    chrome.test.succeed();
  },

  // TODO(crbug.com/427451594): Write a test to check that the parent folder
  // name is in the description for the `SUCCESS` state.
];

chrome.test.runTests(tests);
