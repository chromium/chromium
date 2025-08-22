// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import type {PdfViewerPrivateProxy, ViewerSaveToDriveControlsElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {PdfViewerPrivateProxyImpl} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

const SaveToDriveStatus = chrome.pdfViewerPrivate.SaveToDriveStatus;
const SaveToDriveErrorType = chrome.pdfViewerPrivate.SaveToDriveErrorType;

export class TestPdfViewerPrivateProxy extends TestBrowserProxy implements
    PdfViewerPrivateProxy {
  onSaveToDriveProgress: FakeChromeEvent;
  streamUrl: string = '';

  constructor() {
    super([
      'saveToDrive',
      'setPdfDocumentTitle',
    ]);

    this.onSaveToDriveProgress = new FakeChromeEvent();
  }

  sendSaveToDriveProgress(
      progress: chrome.pdfViewerPrivate.SaveToDriveProgress): void {
    this.onSaveToDriveProgress.callListeners(this.streamUrl, progress);
  }

  saveToDrive(saveRequestType: chrome.pdfViewerPrivate.SaveRequestType): void {
    this.methodCalled('saveToDrive', saveRequestType);
  }

  setPdfDocumentTitle(title: string): void {
    this.methodCalled('setPdfDocumentTitle', title);
  }

  setStreamUrl(streamUrl: string): void {
    this.streamUrl = streamUrl;
  }
}

// Unit tests for the pdf-viewer Save to Drive elements.
const tests = [
  async function testSaveToDriveButton() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.$.toolbar;
    const button = toolbar.getSaveToDriveBubbleAnchor() as
        ViewerSaveToDriveControlsElement;
    const privateProxy = new TestPdfViewerPrivateProxy();
    privateProxy.setStreamUrl(viewer.getStreamUrlForTesting());
    PdfViewerPrivateProxyImpl.setInstance(privateProxy);
    viewer.setOnSaveToDriveProgressListenerForTesting();

    chrome.test.assertTrue(!!button);
    chrome.test.assertEq('pdf:add-to-drive', button.$.save.ironIcon);

    privateProxy.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.NO_ERROR,
      uploadedBytes: 25,
      fileSizeBytes: 100,
    });
    await microtasksFinished();
    chrome.test.assertEq(25, button.progress);
    chrome.test.assertEq('pdf:arrow-upward-alt', button.$.save.ironIcon);

    privateProxy.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.OFFLINE,
      uploadedBytes: 50,
      fileSizeBytes: 100,
    });
    await microtasksFinished();
    chrome.test.assertEq(0, button.progress);
    chrome.test.assertEq('pdf:add-to-drive', button.$.save.ironIcon);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
